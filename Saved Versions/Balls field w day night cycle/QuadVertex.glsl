#version 460 core

layout(location = 0) in vec3 vertexPosition_worldspace;
layout(location = 1) in vec2 vertexUV;

out vec2 uv;

void main() {
	gl_Position.xyz = vertexPosition_worldspace;
	gl_Position.w = 1;

	uv = vertexUV;
}