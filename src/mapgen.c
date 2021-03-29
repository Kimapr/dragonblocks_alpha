#include <stdlib.h>
#include <perlin/perlin.h>
#include "mapgen.h"

int seed = 0;
static Server *server = NULL;

// mapgen prototype
static void generate_block(MapBlock *block)
{
	for (u8 x = 0; x < 16; x++) {
		s32 abs_x = x + block->pos.x * 16;
		for (u8 z = 0; z < 16; z++) {
			s32 abs_z = z + block->pos.z * 16;
			s32 height = noise2d(abs_x / 16, abs_z / 16, 1, seed) * 16;
			for (u8 y = 0; y < 16; y++) {
				s32 abs_y = y + block->pos.y * 16;
				Node type;
				if (abs_y > height)
					type = NODE_AIR;
				else if (abs_y == height)
					type = NODE_GRASS;
				else if (abs_y >= height - 4)
					type = NODE_DIRT;
				else
					type = NODE_STONE;
				block->data[x][y][z] = map_node_create(type);
			}
		}
	}
	ITERATE_LIST(&server->clients, pair) {
		Client *client = pair->value;
		if (client->state == CS_ACTIVE) {
			pthread_mutex_lock(&client->mtx);
			(void) (write_u32(client->fd, CC_BLOCK) && map_serialize_block(client->fd, block));
			pthread_mutex_unlock(&client->mtx);
		}
	}
	block->ready = true;
}

void mapgen_init(Server *srv)
{
	server = srv;
	server->map->on_block_create = &generate_block;
}

static void *mapgen_thread(void *cliptr)
{
	Client *client = cliptr;

	while (client->state != CS_DISCONNECTED) {
		v3s32 pos = map_node_to_block_pos((v3s32) {client->pos.x, client->pos.y, client->pos.z}, NULL);
		for (s32 x = pos.x - 5; x < pos.x + 5; x++)
			for (s32 y = pos.y - 5; y < pos.y + 5; y++)
				for (s32 z = pos.z - 5; z < pos.z + 5; z++)
					map_get_block(client->server->map, (v3s32) {x, y, z}, true);
	}

	return NULL;
}

void mapgen_start_thread(Client *client)
{
	(void) client;
	(void) mapgen_thread;
	pthread_t thread;
	pthread_create(&thread, NULL, &mapgen_thread, client);
}
