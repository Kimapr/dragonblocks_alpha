#ifndef _MESH_H_
#define _MESH_H_

#include <GL/glew.h>
#include <GL/gl.h>
#include <linmath.h/linmath.h>
#include <stdbool.h>
#include "shaders.h"

typedef struct
{
	vec3 pos, rot, scale;
	float angle;
	mat4x4 transform;
	GLuint VAO, VBO;
	bool remove;
	GLfloat *vertices;
	GLsizei size, count;
} Mesh;

Mesh *mesh_create(GLfloat *vertices, GLsizei size);
void mesh_delete(Mesh *mesh);
void mesh_transform(Mesh *mesh);
void mesh_render(Mesh *mesh, ShaderProgram *prog);

#endif
