#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <linmath.h/linmath.h>
#include "shaders.h"
#include "types.h"

void init_camera(GLFWwindow *window, ShaderProgram *prog);
void set_camera_position(v3f pos);
void set_camera_angle(float yaw, float pitch);
void set_window_size(int width, int height);

extern struct movement_dirs
{
	vec3 front;
	vec3 right;
	vec3 up;
} movement_dirs;

#endif