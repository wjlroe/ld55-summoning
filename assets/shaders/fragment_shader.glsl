#version 150 core

in vec2 TexCoords;

out vec4 OutColor;

uniform int settings;
uniform sampler2D fontTexture;
uniform vec4 color;
uniform float radius;
uniform vec2 dimensions;

const int SAMPLE_TEXTURE = (1 << 0);
const int ROUNDED_RECT   = (1 << 1);

void main() {
    if (bool(settings & ROUNDED_RECT)) {
		vec2 coords = a_uv * u_dimensions;
    if (length(coords - vec2(0) < u_radius ||
        length(coords - vec2(0, u_dimensions.y) < u_radius ||
        length(coords - vec2(u_dimensions.x, 0) < u_radius ||
        length(coords - u_dimensions) < u_radius) {
        discard;
    }
    vec4 other_color = color;
    if (bool(settings & SAMPLE_TEXTURE)) {
    	vec4 texture_color = texture(fontTexture, TexCoords);
	    float final_alpha = other_color.a * texture_color.r;
	    OutColor = vec4(other_color.rgb, final_alpha);
    } else {
        OutColor = other_color;
    }
}