#include "client/client_item.h"
#include "client/interact.h"
#include "node.h"

static bool use_dig(__attribute__((unused)) ItemStack *stack)
{
	return interact_pointed.exists
		&& (node_def[interact_pointed.node].dig_class & item_def[stack->type].dig_class);
}

ClientItemDef client_item_def[COUNT_ITEM] = {
	// unknown
	{
		.mesh_path = RESSOURCE_PATH "meshes/unknown.txt",
		.mesh = {0},
		.use = NULL,
	},
	// none
	{
		.mesh_path = NULL,
		.mesh = {0},
		.use = NULL,
	},
	// pickaxe
	{
		.mesh_path = RESSOURCE_PATH "meshes/pickaxe.txt",
		.mesh = {0},
		.use = &use_dig,
	},
	// axe
	{
		.mesh_path = RESSOURCE_PATH "meshes/axe.txt",
		.mesh = {0},
		.use = &use_dig,
	},
	// shovel
	{
		.mesh_path = RESSOURCE_PATH "meshes/shovel.txt",
		.mesh = {0},
		.use = &use_dig,
	},
};

void client_item_init()
{
	for (ItemType i = 0; i < COUNT_ITEM; i++)
		if (client_item_def[i].mesh_path)
			mesh_load(&client_item_def[i].mesh, client_item_def[i].mesh_path);
}

void client_item_deinit()
{
	for (ItemType i = 0; i < COUNT_ITEM; i++)
		if (client_item_def[i].mesh_path)
			mesh_destroy(&client_item_def[i].mesh);
}

Mesh *client_item_mesh(ItemType type)
{
	return client_item_def[type].mesh_path ? &client_item_def[type].mesh : NULL;
}
