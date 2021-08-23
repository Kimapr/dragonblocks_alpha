#version 460 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexTextureCoords;

out vec2 fragmentTextureCoords;

uniform mat4 projection;
uniform mat4 model;

void main()
{
    gl_Position = projection * model * vec4(vertexPosition, 0.0, 1.0);
    fragmentTextureCoords = vertexTextureCoords;
}
