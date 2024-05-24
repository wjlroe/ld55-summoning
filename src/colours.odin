package main

import "core:math"
import "core:strconv"

Color :: distinct v4

WHITE          := color_from_rgba(255, 255, 255, 255)
BLUE           := color_from_rgba(10, 10, 255, 255)
RED            := color_from_rgba(255, 10, 10, 255)
VERY_DARK_BLUE := color_from_rgba(4, 8, 13, 255)
GREEN          := color_from_rgba(10, 255, 10, 255)
AMBER          := color_from_rgba(255, 191, 0, 255)

// float_to_int_color :: proc(color: Color) -> v4s {
// 	return v4s{
// 		int(math.round(color.r * 255.0)),
// 		int(math.round(color.g * 255.0)),
// 		int(math.round(color.b * 255.0)),
// 		int(math.round(color.a * 255.0)),
// 	}
// }

hex_to_float :: proc(hex: string) -> f32 {
	n, ok := strconv.parse_u64_of_base(hex, 16)
	if ok {
		return f32(n)
	}
	return 0.0
}

color_from_hex :: proc(hex: string) -> Color {
	return {
		hex_to_float(hex[1:3]) / 255.0,
		hex_to_float(hex[3:5]) / 255.0,
		hex_to_float(hex[5:7]) / 255.0,
		1.0,
	}
}

color_from_rgba :: #force_inline proc(r, g, b, a: u8) -> Color {
	return {
		f32(r) / 255.0,
		f32(g) / 255.0,
		f32(b) / 255.0,
		f32(a) / 255.0,
	}
}

rgb_to_hsl :: proc(rgb: v3) -> v3 {
	min_color := min(rgb.r, rgb.g, rgb.b)
	max_color := max(rgb.r, rgb.g, rgb.b)
	luminance := (min_color + max_color) / 2.0
	saturation : f32
	if min_color == max_color {
		saturation = 0.0
	} else {
		if luminance <= 0.5 {
			saturation = (max_color - min_color) / (max_color + min_color)
		} else {
			saturation = (max_color - min_color) / (2.0 - max_color - min_color)
		}
	}
	hue : f32
	if rgb.r == rgb.g && rgb.g == rgb.b {
		hue = 0.0
	} else if rgb.r == max_color {
		hue = (rgb.g - rgb.b) / (max_color - min_color)
	} else if rgb.g == max_color {
		hue = 2.0 + (rgb.b - rgb.r) / (max_color - min_color)
	} else if rgb.b == max_color {
		hue = 4.0 + (rgb.r - rgb.g) / (max_color - min_color)
	}
	hue *= 60.0
	if hue < 0.0 {
		hue += 360.0
	}
	hue /= 360.0
	return v3{hue, saturation, luminance}
}

hsl_to_rgb :: proc(hsl: v3) -> (rgb: Color) {
	hue := hsl.x
	saturation := hsl.y
	luminance := hsl.z
	if saturation == 0.0 {
		rgb.r = luminance
		rgb.g = luminance
		rgb.b = luminance
		return
	} else {
		c := (1.0 - abs(2.0 * luminance - 1.0)) * saturation
		h := hue * 360.0
		x := c * (1.0 - abs(math.mod(h / 60.0, 2.0) - 1.0))
		m := luminance - c / 2.0
		if h < 60.0 {
			rgb.r = m + c
			rgb.g = m + x
			rgb.b = m + 0.0
			return
		} else if h < 120.0 {
			rgb.r = m + x
			rgb.g = m + c
			rgb.b = m + 0.0
			return
		} else if h < 180.0 {
			rgb.r = m + 0.0
			rgb.g = m + c
			rgb.b = m + x
			return
		} else if h < 240.0 {
			rgb.r = m + 0.0
			rgb.g = m + x
			rgb.b = m + c
		} else if h < 300.0 {
			rgb.r = m + x
			rgb.g = m + 0.0
			rgb.b = m + c
			return
		} else {
			rgb.r = m + c
			rgb.g = m + 0.0
			rgb.b = m + x
			return
		}
	}
	return
}

color_with_alpha :: proc(color: Color, alpha: f32) -> Color {
	c := color
	c.a = alpha
	return c
}
