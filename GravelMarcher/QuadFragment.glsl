#version 460 core

in vec2 uv;

uniform sampler2D myTextureSampler;

out vec3 color;

void main() {
	color = texture(myTextureSampler, uv).rgb;
}