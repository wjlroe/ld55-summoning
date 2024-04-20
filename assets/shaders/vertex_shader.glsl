#version 150 core

in vec3 position;
in vec2 texture;
in vec4 in_color;

out vec2 TexCoords;
out vec4 Color;

uniform vec3 position_offset;
uniform mat4 ortho;

void main() {
  gl_Position = ortho * vec4(position + position_offset, 1.0);
  TexCoords = texture;
  Color = in_color;
}