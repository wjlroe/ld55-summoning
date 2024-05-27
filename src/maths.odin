package main

import "base:intrinsics"
import "core:fmt"
import "core:math"

// Integer vectors
// TODO: switch to using linalg_glsl.ivec2 etc.
v2s :: distinct [2]i32
v3s :: distinct [3]i32
v4s :: distinct [4]i32

// Floating point vectors
v2 :: distinct [2]f32
v3 :: distinct [3]f32
v4 :: distinct [4]f32

vec_as_string :: proc{
	vec2_as_string_floats, vec3_as_string_floats, vec4_as_string_floats,
	vec2_as_string_ints, vec3_as_string_ints, vec4_as_string_ints,
}

vec2_as_string_floats :: proc(vec: v2) -> string {
	return fmt.tprintf("[%.2f, %.2f]", vec.x, vec.y)
}

vec3_as_string_floats :: proc(vec: v3) -> string {
	return fmt.tprintf("[%.2f, %.2f, %.2f]", vec.x, vec.y, vec.z)
}

vec4_as_string_floats :: proc(vec: v4) -> string {
	return fmt.tprintf("[%.2f, %.2f, %.2f, %.2f]", vec.x, vec.y, vec.z, vec.w)
}

vec2_as_string_ints :: proc(vec: v2s) -> string {
	return fmt.tprintf("[%d, %d]", vec.x, vec.y)
}

vec3_as_string_ints :: proc(vec: v3s) -> string {
	return fmt.tprintf("[%d, %d, %d]", vec.x, vec.y, vec.z)
}

vec4_as_string_ints :: proc(vec: v4s) -> string {
	return fmt.tprintf("[%d, %d, %d, %d]", vec.x, vec.y, vec.z, vec.w)
}

vec_ints_to_floats :: proc(vec: v2s) -> v2 {
	return v2{f32(vec.x), f32(vec.y)}
}

vec_floats_to_ints :: proc(vec: v2) -> v2s {
	return v2s{i32(math.round(vec.x)), i32(math.round(vec.y))}
}

rectangle2s :: struct {
	min: v2s,
	max: v2s,
}

rectangle2 :: struct {
	min: v2,
	max: v2,
}

rect_min_dim :: proc{rect_min_dim_v2s, rect_min_dim_v2}

rect_min_dim_v2s :: proc "contextless" (min, dim: v2s) -> rectangle2s {
	rect : rectangle2s = ---
	rect.min = min
	rect.max = min + dim
	return rect
}

rect_min_dim_v2 :: proc "contextless" (min, dim: v2) -> rectangle2 {
	rect : rectangle2 = ---
	rect.min = min
	rect.max = min + dim
	return rect
}

rect_from_points_i32 :: proc "contextless" (x0, y0, x1, y1: i32) -> rectangle2s {
	rect : rectangle2s = ---
	rect.min = v2s{x0, y0}
	rect.max = v2s{x1, y1}
	return rect
}

rect_add :: proc{rect_add_floats}

rect_add_floats :: proc "contextless" (rect: ^rectangle2, vec: v2) {
	rect.min += vec
	rect.max += vec
}

rect_sub :: proc{rect_sub_floats}

rect_sub_floats :: proc "contextless" (rect: ^rectangle2, vec: v2) {
	rect.min -= vec
	rect.max -= vec
}

rect_div :: proc{rect_div_floats, rect_div_ints}

rect_div_floats :: proc "contextless" (rect: ^rectangle2, vec: v2) {
	rect.min /= vec
	rect.max /= vec
}

rect_div_ints :: proc "contextless" (rect: ^rectangle2s, vec: v2s) {
	rect.min /= vec
	rect.max /= vec
}

rect_width :: proc{rect_width_floats, rect_width_ints}

rect_width_floats :: proc "contextless" (rect: rectangle2) -> f32 {
	return rect.max.x - rect.min.x
}

rect_width_ints :: proc "contextless" (rect: rectangle2s) -> i32 {
	return rect.max.x - rect.min.x
}

rect_height :: proc{rect_height_floats, rect_height_ints}

rect_height_floats :: proc "contextless" (rect: rectangle2) -> f32 {
	return rect.max.y - rect.min.y
}

rect_height_ints :: proc "contextless" (rect: rectangle2s) -> i32 {
	return rect.max.y - rect.min.y
}

rect_dim :: proc{rect_dim_floats, rect_dim_ints}

rect_dim_floats :: proc "contextless" (rect: rectangle2) -> v2 {
	return v2{
		rect_width(rect),
		rect_height(rect),
	}
}

rect_dim_ints :: proc "contextless" (rect: rectangle2s) -> v2s {
	return v2s{
		rect_width(rect),
		rect_height(rect),
	}
}

rect_centre :: proc{rect_centre_floats, rect_centre_ints}

rect_centre_floats :: proc "contextless" (rect: rectangle2) -> v2 {
	return v2{
		(rect.max.x + rect.min.x) / 2.0,
		(rect.max.y + rect.min.y) / 2.0,
	}
}

rect_centre_ints :: proc "contextless" (rect: rectangle2s) -> v2s {
	return v2s{
		(rect.min.x + rect.max.x) / 2,
		(rect.min.y + rect.max.y) / 2,
	}
}

is_in_rectangle :: proc{is_in_rectangle_int, is_in_rectangle_float}

is_in_rectangle_int :: proc "contextless" (rect: rectangle2s, point: v2s) -> bool {
	return ((point.x >= rect.min.x) &&
			(point.y >= rect.min.y) &&
			(point.x <= rect.max.x) &&
			(point.y <= rect.max.y))
}

is_in_rectangle_float :: proc "contextless" (rect: rectangle2, point: v2) -> bool {
	return ((point.x >= rect.min.x) &&
			(point.y >= rect.min.y) &&
			(point.x <= rect.max.x) &&
			(point.y <= rect.max.y))
}

vec_distance_from_rect :: proc(vec: v2, rect: rectangle2) -> v2 {
	centre := rect_centre(rect)
	return v2{abs(centre.x - vec.x), abs(centre.y - vec.y)}
}

rectangles_overlap :: proc "contextless" (rect1, rect2: rectangle2) -> bool {
	return (is_in_rectangle(rect1, rect2.min) ||
			is_in_rectangle(rect1, rect2.max) ||
			is_in_rectangle(rect2, rect1.min) ||
			is_in_rectangle(rect2, rect1.max))
}

rectangle_contained_within :: proc "contextless" (inner, outer: rectangle2) -> bool {
	return (is_in_rectangle(outer, inner.min) &&
			is_in_rectangle(outer, inner.max))
}

rect_as_string :: proc{rect_as_string_floats}

rect_as_string_floats :: proc(rect: rectangle2) -> string {
	return fmt.tprintf("%s -> %s", vec_as_string(rect.min), vec_as_string(rect.max))
}

rect_ints_to_floats :: proc(rect: rectangle2s) -> rectangle2 {
	return rectangle2{
		vec_ints_to_floats(rect.min),
		vec_ints_to_floats(rect.max),
	}
}

rect_floats_to_ints :: proc(rect: rectangle2) -> rectangle2s {
	return rectangle2s{
		vec_floats_to_ints(rect.min),
		vec_floats_to_ints(rect.max),
	}
}

ortho_matrix :: proc "contextless" (min, max: v3) -> matrix[4,4]f32 {
	xdiff := max.x - min.x
	ydiff := max.y - min.y
	zdiff := max.z - min.z
	ortho := matrix[4,4]f32{
		2.0 / xdiff, 0.0, 0.0, 0.0,
		0.0, 2.0 / ydiff, 0.0, 0.0,
		0.0, 0.0, -2.0 / zdiff, 0.0,
		-((max.x+min.x)/xdiff), -((max.y+min.y)/ydiff), -((max.z+min.z)/zdiff), 1.0,
	}
	// FIXME: untranspose the above
	return intrinsics.transpose(ortho)
}

transform_for_position :: proc(position, within: rectangle2) -> matrix[4,4]f32 {
	pos_width := rect_width(position)
	within_width := rect_width(within)
	pos_height := rect_height(position)
	within_height := rect_height(within)
	scale := scale_matrix(pos_width / within_width, pos_height / within_height, 1.0)
	pos_centre := rect_centre(position)
	translation := translation_matrix((pos_centre.x / within_width) * 2.0 - 1.0, -((pos_centre.y / within_height) * 2.0 - 1.0), 0.0)
	return translation * scale
}

screen_transform_for_position :: proc(position, within: rectangle2) -> matrix[4,4]f32 {
	pos_width := rect_width(position)
	pos_height := rect_height(position)
	within_width := rect_width(within)
	within_height := rect_height(within)
	pos_center := rect_centre(position)

	scale := scale_matrix((pos_width / within_width) * (within_width / 2.0), -(pos_height / within_height) * (within_height / 2.0), 1.0)
	translation := translation_matrix(pos_center.x, pos_center.y, 0.0)
	return translation * scale
}

scale_matrix :: proc(x, y, z: f32) -> matrix[4,4]f32 {
	scale := matrix[4,4]f32{
		x, 0.0, 0.0, 0.0,
		0.0, y, 0.0, 0.0,
		0.0, 0.0, z, 0.0,
		0.0, 0.0, 0.0, 1.0,
	}
	return scale
}

translation_matrix :: proc(x, y, z: f32) -> matrix[4,4]f32{
	translation := matrix[4,4]f32{
		1.0, 0.0, 0.0, x,
		0.0, 1.0, 0.0, y,
		0.0, 0.0, 1.0, z,
		0.0, 0.0, 0.0, 1.0,
	}
	return translation
}

transform_for_font_texture :: proc(texture: rectangle2) -> matrix[4,4]f32 {
	assert(rect_width(texture) > 0.0)
	texture_rect := rectangle2{
		{0.0, 0.0},
		{1.0, 1.0},
	}
	tex_coords_width := rect_width(texture)
	tex_coords_height := rect_height(texture)
	tex_width := rect_width(texture_rect)
	tex_height := rect_height(texture_rect)
	scale := scale_matrix(tex_coords_width / tex_width, tex_coords_height / tex_height, 1.0)
	translation := translation_matrix(texture.min.x / tex_width, texture.min.y / tex_height, 0.0)
	return translation * scale
}

rect_debug_string :: proc(rect: rectangle2) -> string {
	width := rect_width(rect)
	height := rect_height(rect)
	centre := rect_centre(rect)
	return fmt.tprintf("Centre: [%.3f,%.3f], bounds: [%.3f,%.3f]", centre.x, centre.y, width, height)
}
