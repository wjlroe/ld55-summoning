#version 150 core

in vec2 TexCoords;
in vec2 screen_coords;
in vec4 gl_FragCoord;

out vec4 OutColor;

uniform int settings;
uniform sampler2D fontTexture;
uniform vec4 color;
uniform float radius;
uniform vec2 dimensions;
uniform vec2 origin;

const int SAMPLE_TEXTURE = (1 << 0);
const int ROUNDED_RECT   = (1 << 1);

// https://www.shadertoy.com/view/WtdSDs
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
    
    if (bool(settings & SAMPLE_TEXTURE)) {
    	vec4 texture_color = texture(fontTexture, TexCoords);
	    float final_alpha = output_color.a * texture_color.r;
	    output_color = vec4(output_color.rgb, final_alpha);
    }

	if (bool(settings & ROUNDED_RECT)) {
		// vec2 coords = TexCoords * dimensions;
    	// if (length(coords - vec2(0)) > radius ||
        //	length(coords - vec2(0, dimensions.y)) > radius ||
        //	length(coords - vec2(dimensions.x, 0)) > radius ||
        //	length(coords - dimensions) > radius) {
        //		discard;
    	//}
		// float distance = rounded_box_sdf(gl_FragCoord.xy - origin, dimensions / 2.0f, radius);

		float edge_softness = 1.0f;
		vec2 softness_padding = vec2(max(0.0f, edge_softness * 2.0f - 1.0f),
									 max(0.0f, edge_softness * 2.0f - 1.0f));
		//vec2 coord = gl_FragCoord.xy;
		vec2 coord = screen_coords;
		//coord = vec2(0.0f, 0.0f);
		float dist = rounded_box_sdf(coord, 
									 origin, 
									 dimensions / 2.0f - softness_padding, 
									 radius);
		float sdf_factor = 1.0f - smoothstep(0.0f, 2.0f * edge_softness, dist);
		output_color.a *= sdf_factor;


		//float smoothed_alpha = 1.0f - smoothstep(0.0f, edge_softness * 2.0f, distance);
		// vec4 fadded_out = vec4(output_color.rgb, 0.0f);
		// vec4 fadded_out = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		// output_color = mix(output_color, fadded_out, smoothed_alpha);
		//if (TexCoords.x < -0.5) { output_color.g = 0.0; }
		// output_color.a *= smoothed_alpha;
		//output_color = output_color * vec4(1.0f, 1.0f, 1.0f, smoothed_alpha);
	}

    OutColor = output_color;
}