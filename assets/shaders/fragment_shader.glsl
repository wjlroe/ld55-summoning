#version 140

in vec2 TexCoords;
in vec4 Color;

out vec4 OutColor;

uniform sampler2D fontTexture;

void main() {
    vec4 other_color = Color;
	vec4 texture_color = texture(fontTexture, TexCoords);
	float final_alpha = other_color.a * texture_color.r;
	OutColor = vec4(other_color.rgb, final_alpha);
}