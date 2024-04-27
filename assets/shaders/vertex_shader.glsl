#version 150 core

in vec3 position;
in vec2 texture;

out vec2 TexCoords;
out vec2 screen_coords;

uniform vec2 position_offset;
uniform mat4 ortho;

void main() {
  screen_coords = vec2(position) + position_offset;
  gl_Position = ortho * vec4(position + vec3(position_offset, 0.0), 1.0);
  TexCoords = texture;
}