#include <stdio.h>
#include <stdlib.h>
#include "client/frustum.h"
#include "client/object.h"
#include "client/scene.h"
#define OBJECT_VERTEX_ATTRIBUTES 5

static VertexAttribute vertex_attributes[OBJECT_VERTEX_ATTRIBUTES] = {
	// position
	{
		.type = GL_FLOAT,
		.length = 3,
		.size = sizeof(Vertex3DPosition),
	},
	// normal
	{
		.type = GL_FLOAT,
		.length = 3,
		.size = sizeof(Vertex3DNormal),
	},
	// textureIndex
	{
		.type = GL_FLOAT,
		.length = 1,
		.size = sizeof(Vertex3DTextureIndex),
	},
	// textureCoordinates
	{
		.type = GL_FLOAT,
		.length = 2,
		.size = sizeof(Vertex3DTextureCoordinates),

	},
	// color
	{
		.type = GL_FLOAT,
		.length = 3,
		.size = sizeof(Vertex3DColor),
	},
};

static VertexLayout vertex_layout = {
	.attributes = vertex_attributes,
	.count = OBJECT_VERTEX_ATTRIBUTES,
	.size = sizeof(Vertex3D),
};

Object *object_create()
{
	Object *obj = malloc(sizeof(Object));
	obj->pos = (v3f32) {0.0f, 0.0f, 0.0f};
	obj->rot = (v3f32) {0.0f, 0.0f, 0.0f};
	obj->scale = (v3f32) {1.0f, 1.0f, 1.0f};
	obj->angle = 0.0f;
	obj->remove = false;
	obj->meshes = NULL;
	obj->meshes_count = 0;
	obj->visible = true;
	obj->wireframe = false;
	obj->frustum_culling = false;
	obj->transparent = false;
	obj->box = (aabb3f32) {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
	obj->current_face = NULL;
	obj->faces = array_create(sizeof(ObjectFace));
	obj->on_render = NULL;
	obj->extra = NULL;

	return obj;
}

void object_delete(Object *obj)
{
	for (size_t i = 0; i < obj->meshes_count; i++)
		mesh_delete(obj->meshes[i]);

	free(obj);
}

void object_set_texture(Object *obj, Texture *texture)
{
	if (obj->current_face && obj->current_face->texture == texture->id)
		return;

	ObjectFace face = {
		.texture = texture->id,
		.vertices = array_create(sizeof(Vertex3D)),
	};

	array_append(&obj->faces, &face);
	obj->current_face = &((ObjectFace *) obj->faces.ptr)[obj->faces.siz - 1];
}

void object_add_vertex(Object *obj, Vertex3D *vertex)
{
	array_append(&obj->current_face->vertices, vertex);
}

static int qsort_compare_faces(const void *f1, const void *f2)
{
	return ((ObjectFace *) f1)->texture - ((ObjectFace *) f2)->texture;
}

static void add_mesh(Array *meshes, Array *vertices, Array *textures)
{
	if (vertices->siz > 0) {
		Mesh *mesh = mesh_create();
		mesh->vertices = vertices->ptr;
		mesh->vertices_count = vertices->siz;
		mesh->free_vertices = true;
		mesh->free_textures = true;
		mesh->layout = &vertex_layout;
		size_t textures_count;
		array_copy(textures, (void *) &mesh->textures, &textures_count);
		mesh->textures_count = textures_count;

		free(textures->ptr);

		array_append(meshes, &mesh);
	}

	*vertices = array_create(sizeof(Vertex3D));
	*textures = array_create(sizeof(GLuint));
}

bool object_add_to_scene(Object *obj)
{
	if (obj->faces.siz == 0)
		return false;

	object_transform(obj);

	qsort(obj->faces.ptr, obj->faces.siz, sizeof(ObjectFace), &qsort_compare_faces);

	GLuint max_texture_units = scene_get_max_texture_units();

	Array meshes = array_create(sizeof(Mesh *));

	Array vertices = array_create(sizeof(Vertex3D));
	Array textures = array_create(sizeof(GLuint));

	GLuint texture_id = 0;
	GLuint texture_index = max_texture_units;

	for (size_t f = 0; f < obj->faces.siz; f++) {
		ObjectFace *face = &((ObjectFace *) obj->faces.ptr)[f];

		if (face->texture != texture_id) {
			if (++texture_index >= max_texture_units) {
				add_mesh(&meshes, &vertices, &textures);
				texture_index = 0;
			}

			texture_id = face->texture;
			array_append(&textures, &texture_id);
		}

		for (size_t v = 0; v < face->vertices.siz; v++) {
			Vertex3D *vertex = &((Vertex3D *) face->vertices.ptr)[v];
			vertex->textureIndex = texture_index;
			array_append(&vertices, vertex);
		}

		free(face->vertices.ptr);
	}
	add_mesh(&meshes, &vertices, &textures);
	free(obj->faces.ptr);

	array_copy(&meshes, (void *) &obj->meshes, &obj->meshes_count);
	free(meshes.ptr);

	scene_add_object(obj);

	return true;
}

void object_transform(Object *obj)
{
	mat4x4_translate(obj->transform, obj->pos.x, obj->pos.y, obj->pos.z);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	mat4x4_rotate(obj->transform, obj->transform, obj->rot.x, obj->rot.y, obj->rot.z, obj->angle);
	mat4x4_scale_aniso(obj->transform, obj->transform, obj->scale.x, obj->scale.y, obj->scale.z);
#pragma GCC diagnostic pop
}

bool object_before_render(Object *obj, f64 dtime)
{
	if (obj->on_render)
		obj->on_render(obj, dtime);

	if (! obj->visible)
		return false;

	if (obj->frustum_culling) {
		aabb3f32 box = {v3f32_add(obj->box.min, obj->pos), v3f32_add(obj->box.max, obj->pos)};

		 if (! frustum_is_visible(box))
			return false;
	}

	return true;
}

void object_render(Object *obj)
{
	glUniformMatrix4fv(scene.loc_model, 1, GL_FALSE, obj->transform[0]);

	if (obj->wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (size_t i = 0; i < obj->meshes_count; i++)
		mesh_render(obj->meshes[i]);

	if (obj->wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
