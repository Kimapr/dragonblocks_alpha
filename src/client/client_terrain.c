#define _GNU_SOURCE // don't worry, GNU extensions are only used when available
#include <dragonstd/queue.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "client/client.h"
#include "client/facecache.h"
#include "client/client_config.h"
#include "client/client_node.h"
#include "client/client_player.h"
#include "client/client_terrain.h"
#include "client/debug_menu.h"
#include "client/terrain_gfx.h"
#include "facedir.h"

#define MAX_REQUESTS 4

Terrain *client_terrain;

static atomic_bool cancel;         // used to notify meshgen and sync thread about quit
static Queue meshgen_tasks;        // TerrainCHunk * queue (thread safe)
static pthread_t *meshgen_threads; // consumer threads for meshgen queue
static pthread_t sync_thread;      // this thread requests new / changed chunks from server
static u32 load_distance;          // load distance sent by server
static size_t load_chunks;         // cached number of facecache positions to process every sync step (matches load distance)

// meshgen functions

// dequeue callback to update queue state in a thread safe manner
static TerrainChunk *set_dequeued(TerrainChunk *chunk)
{
	pthread_mutex_lock(&chunk->mtx);
	((TerrainChunkMeta *) chunk->extra)->queue = false;
	pthread_mutex_unlock(&chunk->mtx);

	return chunk;
}

// mesh generator step
static void meshgen_step()
{
	TerrainChunk *chunk = queue_deq(&meshgen_tasks, &set_dequeued);

	if (chunk)
		terrain_gfx_make_chunk_model(chunk);
}

// sync functions

// send chunk request command to server
static void request_chunk(v3s32 pos)
{
	dragonnet_peer_send_ToServerRequestChunk(client, &(ToServerRequestChunk) {
		.pos = pos
	});
}

// terrain synchronisation step
static void sync_step()
{
	static u64 tick = 0;
	static v3s32 *old_requests = NULL;
	static size_t old_num_requests = 0;

	v3f64 player_pos;
	ClientEntity *entity = client_player_entity_local();

	if (entity) {
		pthread_rwlock_rdlock(&entity->lock_pos_rot);
		player_pos = entity->data.pos;
		pthread_rwlock_unlock(&entity->lock_pos_rot);

		refcount_drp(&entity->rc);
	} else {
		sched_yield();
		return;
	}

	v3s32 center = terrain_chunkp((v3s32) {player_pos.x, player_pos.y, player_pos.z});

	u64 last_tick = tick++;

	v3s32 *requests = malloc(MAX_REQUESTS * sizeof *requests);
	size_t num_requests = 0;

	for (size_t i = 0; i < load_chunks; i++) {
		v3s32 pos = v3s32_add(facecache_get(i), center);
		TerrainChunk *chunk = terrain_get_chunk(client_terrain, pos, false);

		if (chunk) {
			pthread_mutex_lock(&chunk->mtx);

			TerrainChunkMeta *meta = chunk->extra;
			switch (meta->state) {
				case CHUNK_READY:
					// re-request chunks that got back into range
					if (meta->sync < last_tick)
						request_chunk(pos);
					__attribute__((fallthrough));

				case CHUNK_FRESH:
					meta->state = CHUNK_READY;
					meta->sync = tick;
					break;

				case CHUNK_RECIEVING:
					break;
			}

			pthread_mutex_unlock(&chunk->mtx);
		} else if (num_requests < MAX_REQUESTS) {
			// avoid duplicate requests
			bool requested = false;

			for (size_t i = 0; i < old_num_requests; i++) {
				if (v3s32_equals(old_requests[i], pos)) {
					requested = true;
					break;
				}
			}

			if (!requested)
				request_chunk(pos);

			requests[num_requests++] = pos;
		}
	}

	if (old_requests)
		free(old_requests);

	old_requests = requests;
	old_num_requests = num_requests;
}

// pthread routine for meshgen and sync thread

static struct LoopThread {
	const char *name;
	void (*step)();
} loop_threads[2] = {
	{"meshgen", &meshgen_step},
	{   "sync",    &sync_step},
};

static void *loop_routine(struct LoopThread *thread)
{
#ifdef __GLIBC__ // check whether bloat is enabled
	pthread_setname_np(pthread_self(), thread->name);
#endif // __GLIBC__

	// warning: extremely advanced logic
	while (!cancel)
		thread->step();

	return NULL;
}

// terrain callbacks
// note: all these functions require the chunk mutex to be locked, which is always the case when a terrain callback is invoked

// callback for initializing a newly created chunk
// allocate and initialize meta data
static void on_create_chunk(TerrainChunk *chunk)
{
	TerrainChunkMeta *meta = chunk->extra = malloc(sizeof *meta);

	meta->state = CHUNK_RECIEVING;
	meta->queue = false;
	meta->sync = 0;
	meta->model = NULL;
	meta->empty = false;
	meta->has_model = false;
	for (int i = 0; i < 6; i++)
		meta->depends[i] = false;
}

// callback for deleting a chunk
// free meta data
static void on_delete_chunk(TerrainChunk *chunk)
{
	free(chunk->extra);
}

// callback for determining whether a chunk should be returned by terrain_get_chunk
// hold back chunks that have not been fully read from server yet when the create flag is not set
static bool on_get_chunk(TerrainChunk *chunk, bool create)
{
	return create || ((TerrainChunkMeta *) chunk->extra)->state > CHUNK_RECIEVING;
}

// public functions

// called on startup
void client_terrain_init()
{
	client_terrain = terrain_create();
	client_terrain->callbacks.create_chunk = &on_create_chunk;
	client_terrain->callbacks.delete_chunk = &on_delete_chunk;
	client_terrain->callbacks.get_chunk    = &on_get_chunk;
	client_terrain->callbacks.delete_node  = &client_node_delete;

	cancel = false;
	queue_ini(&meshgen_tasks);

	client_terrain_set_load_distance(10); // some initial fuck idk just in case server is stupid

	sync_thread = 0;
	meshgen_threads = malloc(sizeof *meshgen_threads * client_config.meshgen_threads);
	for (unsigned int i = 0; i < client_config.meshgen_threads; i++)
		meshgen_threads[i] = 0; // but why???
}

// called on shutdown
void client_terrain_deinit()
{
	queue_clr(&meshgen_tasks, NULL, NULL, NULL);
	terrain_delete(client_terrain);
}

// start meshgen and sync threads
void client_terrain_start()
{
	for (unsigned int i = 0; i < client_config.meshgen_threads; i++)
		pthread_create(&meshgen_threads[i], NULL, (void *) &loop_routine, &loop_threads[0]);

	pthread_create(&sync_thread, NULL, (void *) &loop_routine, &loop_threads[1]);
}

// stop meshgen and sync threads
void client_terrain_stop()
{
	cancel = true;
	queue_cnl(&meshgen_tasks);

	for (unsigned int i = 0; i < client_config.meshgen_threads; i++)
		if (meshgen_threads[i])
			pthread_join(meshgen_threads[i], NULL);
	free(meshgen_threads);

	if (sync_thread)
		pthread_join(sync_thread, NULL);
}

// update load distance
void client_terrain_set_load_distance(u32 dist)
{
	load_distance = dist;
	load_chunks = facecache_count(load_distance);
	debug_menu_changed(ENTRY_LOAD_DISTANCE);
}

// return load distance
u32 client_terrain_get_load_distance()
{
	return load_distance;
}

// called when a chunk was recieved from server
void client_terrain_chunk_received(TerrainChunk *chunk)
{
	pthread_mutex_lock(&chunk->mtx);
	TerrainChunkMeta *meta = chunk->extra;

	if (meta->state == CHUNK_RECIEVING)
		meta->state = CHUNK_FRESH;

	client_terrain_meshgen_task(chunk, true);
	pthread_mutex_unlock(&chunk->mtx);

	for (int i = 0; i < 6; i++) {
		TerrainChunk *neighbor = terrain_get_chunk(client_terrain,
			v3s32_sub(chunk->pos, facedir[i]), false);
		if (!neighbor)
			continue;

		pthread_mutex_lock(&neighbor->mtx);
		TerrainChunkMeta *neighbor_meta = neighbor->extra;
		if (neighbor_meta->depends[i])
			client_terrain_meshgen_task(neighbor, true);
		pthread_mutex_unlock(&neighbor->mtx);
	}
}

// enqueue chunk to mesh update queue
void client_terrain_meshgen_task(TerrainChunk *chunk, bool changed)
{
	TerrainChunkMeta *meta = chunk->extra;
	if (meta->queue)
		return;

	if (meta->empty) {
		meta->has_model = true;

		if (meta->model) {
			meta->model->flags.delete = 1;
			meta->model = NULL;
		}
	} else {
		meta->queue = true;

		if (meta->has_model && changed)
			queue_ppd(&meshgen_tasks, chunk);
		else
			queue_enq(&meshgen_tasks, chunk);
	}
}
