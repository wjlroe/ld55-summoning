#version 150 core

in vec3 position;
in vec2 texture;

out vec2 TexCoords;

uniform vec2 position_offset;
uniform mat4 ortho;

void main() {
  gl_Position = ortho * vec4(position + vec3(position_offset, 0.0), 1.0);
  TexCoords = texture;
}