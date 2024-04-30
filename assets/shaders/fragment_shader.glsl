#version 150 core

in vec2 TexCoords;

out vec4 OutColor;

uniform int settings;
uniform sampler2D fontTexture;
uniform vec4 color;
uniform float radius;
uniform vec2 dimensions;
uniform vec2 origin;

const int SAMPLE_FONT_TEXTURE   = (1 << 0);
const int ROUNDED_RECT          = (1 << 1);
const int SAMPLE_BITMAP_TEXTURE = (1 << 2);

float rounded_box_sdf(vec2 sample_pos, 
					  vec2 rect_centre, 
					  vec2 rect_half_size, 
					  float radius) {
	vec2 d2 = (abs(rect_centre - sample_pos) -
				rect_half_size + 
				vec2(radius, radius));
	return min(max(d2.x, d2.y), 0.0f) + length(max(d2, vec2(0.0f, 0.0f))) - radius;
}

void main() {
    vec4 output_color = color;
    
    if (bool(settings & SAMPLE_FONT_TEXTURE)) {
    	vec4 texture_color = texture(fontTexture, TexCoords);
	    float final_alpha = output_color.a * texture_color.r;
	    output_color = vec4(output_color.rgb, final_alpha);
    }

	if (bool(settings & SAMPLE_BITMAP_TEXTURE)) {
		vec4 texture_color = texture(fontTexture, TexCoords);
		output_color = texture_color;
	}

	if (bool(settings & ROUNDED_RECT)) {
		float edge_softness = 1.0f;
		vec2 softness_padding = vec2(max(0.0f, edge_softness * 2.0f - 1.0f),
									 max(0.0f, edge_softness * 2.0f - 1.0f));
		vec2 coord = gl_FragCoord.xy;
		float dist = rounded_box_sdf(coord,
									 origin,
									 dimensions / 2.0f - softness_padding,
									 radius);
		float sdf_factor = 1.0f - smoothstep(0.0f, 2.0f * edge_softness, dist);
		output_color.a *= sdf_factor;
	}

    OutColor = output_color;
}