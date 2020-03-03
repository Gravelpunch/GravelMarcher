#version 460 core

layout(location = 0) in vec3 vertexLocation_screen;
layout(location = 1) in vec3 vertexUV;
layout(location = 2) in vec3 vertexRay_view;

out vec3 rayView;

void main() {
	gl_Position.xyz = vertexLocation_screen;
	gl_Position.w = 1;

	rayView = vertexRay_view;
}