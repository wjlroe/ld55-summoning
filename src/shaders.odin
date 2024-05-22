package main

import "core:log"
import gl "vendor:OpenGL"
import "core:reflect"
import "base:runtime"
import "core:container/small_array"
import "core:strings"

Shader_Param_Type :: enum {
	UInt32,
	Int32,
	Float32,
	Vec2,
	Vec3,
	Vec4,
	Matrix4,
	Texture2D,
}

shader_param_type_from_gl_type :: proc(type: ^Shader_Param_Type, gl_type: u32) {
	switch gl_type {
		case gl.UNSIGNED_INT: type^ = .UInt32
		case gl.INT: type^ = .Int32
		case gl.FLOAT: type^ = .Float32
		case gl.SAMPLER_2D: type^ = .Texture2D
		case gl.FLOAT_VEC2: type^ = .Vec2
		case gl.FLOAT_VEC3: type^ = .Vec3
		case gl.FLOAT_VEC4: type^ = .Vec4
		case gl.FLOAT_MAT4: type^= .Matrix4
		case: {
			log.errorf("Can't convert GL type {:x}", gl_type)
			assert(false)
		}
	}
}

// NOTE: location == index in uniforms array of Shader
Shader_Param :: struct {
	name: string,
	type: Shader_Param_Type,
	sampler_index: int, // only used for sampler2D
}

MAX_SHADER_ATTRIBUTES :: 16
MAX_SHADER_UNIFORMS   :: 16

Shader :: struct {
	program_id: u32,
	vao:        u32,
	vbo:        u32,
	ebo:        u32,
	attributes: small_array.Small_Array(MAX_SHADER_ATTRIBUTES, Shader_Param),
	uniforms:   small_array.Small_Array(MAX_SHADER_UNIFORMS,   Shader_Param),
}

push_shader_attribute :: proc(shader: ^Shader, attribute: Shader_Param) -> (ok: bool) {
	ok = small_array.push_back(&shader.attributes, attribute)
	assert(ok)
	return
}

push_shader_uniform :: proc(shader: ^Shader, uniform: Shader_Param) -> (ok: bool) {
	ok = small_array.push_back(&shader.uniforms, uniform)
	assert(ok)
	return
}

shader_attribute_location :: proc(shader: Shader, attribute_name: string) -> (location: u32, ok: bool) {
	param : Shader_Param
	for i in 0..<shader.attributes.len {
		param, ok = small_array.get_safe(shader.attributes, i)
		if !ok {
			return
		}
		if param.name == attribute_name {
			location = u32(i)
			ok = true
			return
		}
	}
	ok = false // we didn't find it
	return
}

shader_uniform_location :: proc(shader: Shader, uniform_name: string) -> (location: u32, ok: bool) {
	param : Shader_Param
	for i in 0..<shader.uniforms.len {
		param, ok = small_array.get_safe(shader.uniforms, i)
		if !ok {
			return
		}
		if param.name == uniform_name {
			location = u32(i)
			ok = true
			return
		}
	}
	ok = false // we didn't find it
	return
}

MAX_SHADERS :: 8
global_shaders : small_array.Small_Array(MAX_SHADERS, Shader)

push_shader :: proc(shader: Shader) -> (shader_idx: int, ok: bool) {
	ok = small_array.push_back(&global_shaders, shader)
	assert(ok)
	shader_idx = global_shaders.len - 1
	return
}

get_program_iv :: proc(program_id: u32, parameter: u32) -> int {
	value : i32
	gl.GetProgramiv(program_id, parameter, &value)
	return int(value)
}

// TODO: If we look up attributes by name later, do we need to do this also?
init_shader_attributes :: proc(shader: ^Shader) -> (ok: bool) {
	using shader
	count := get_program_iv(program_id, gl.ACTIVE_ATTRIBUTES)

	buf_size : i32 = 16
	name : [16]byte
	length : i32 // name length
	attribute : Shader_Param

	size : i32 // size of the variable
	type : u32 // type of the variable
	for i in u32(0)..<u32(count) {
		gl.GetActiveAttrib(program_id, i, buf_size, &length, &size, &type, &name[0])
		attribute_name := cstring(&name[0])
		attribute.name = strings.clone_from_cstring(attribute_name)
		shader_param_type_from_gl_type(&attribute.type, type)
		ok = push_shader_attribute(shader, attribute)
		if !ok {
			return
		}
	}

	ok = true
	return
}

// TODO: If we look up uniforms by name later, do we need to do this also?
// NOTE: we use this to locate sampler indexes because this enumerates them!!
init_shader_uniforms :: proc(shader: ^Shader) -> (ok: bool) {
	using shader
	count := u32(get_program_iv(program_id, gl.ACTIVE_UNIFORMS))

	buf_size : i32 : 16
	name : [buf_size]byte
	length : i32 // name length

	size : i32 // size of the variable
	type : u32 // type of the variable
	uniform : Shader_Param
	sampler_index := 0

	i : u32 = 0
	texture_i := 0
	for i < count {
		gl.GetActiveUniform(program_id, i, i32(buf_size), &length, &size, &type, &name[0])
		uniform_name := cstring(&name[0])
		uniform.name = strings.clone_from_cstring(uniform_name)
		shader_param_type_from_gl_type(&uniform.type, type)
		if uniform.type == .Texture2D {
			uniform.sampler_index = sampler_index
			sampler_index += 1
		} else {
			uniform.sampler_index = -1 // not a sampler
		}
		ok = push_shader_uniform(shader, uniform)
		if !ok {
			return
		}
		i += 1
	}
	ok = true
	return
}

check_shader_compilation :: proc(shader_name: string, shader_id: u32) -> (ok: bool) {
	compile_status : i32
	gl.GetShaderiv(shader_id, gl.COMPILE_STATUS, &compile_status)
	gl_true : i32 = 1
	if compile_status != gl_true {
		info_len : i32
		max_len :: 1024
		info_log : [max_len]u8
		gl.GetShaderInfoLog(shader_id, max_len, &info_len, &info_log[0])
		compile_error := info_log[0:info_len]
		log.errorf("Error compiling %s:\n%s", shader_name, compile_error)
		assert(compile_status == gl_true)
		ok = false
	} else {
		ok = true
	}
	return
}

get_program_info :: proc(program_id: u32, object_param: u32) -> (ok: bool, error: string) {
	ok = true
	status := get_program_iv(program_id, object_param)
	gl_true := 1
	if status != gl_true {
		ok = false
		info_len : i32
		max_len :: 1024
		info_log : [max_len]u8
		gl.GetProgramInfoLog(program_id, max_len, &info_len, &info_log[0])
		link_error := string(info_log[0:info_len])
		error = strings.clone(link_error, context.temp_allocator)
	}
	return
}

check_shader_linking :: proc(program_id: u32) -> (ok: bool) {
	link_status, link_error := get_program_info(program_id, gl.LINK_STATUS)
	if !link_status {
		log.errorf("Error linking shader program:\n%s", link_error)
	}
	ok = link_status
	assert(link_status)
	return
}

check_shader_program_valid :: proc(program_id: u32) -> (ok: bool) {
	gl.ValidateProgram(program_id)
	valid_status, valid_error := get_program_info(program_id, gl.VALIDATE_STATUS)
	if !valid_status {
		log.errorf("Error validating shader program:\n%s", valid_error)
	}
	ok = valid_status
	assert(valid_status)
	return
}

// TODO: shader filenames if loaded from file (file:linenum error printouts)
compile_shader_program :: proc(vertex_shader_source: string, fragment_shader_source: string) -> (shader_idx: int, ok: bool) {
	shader := Shader{}
	using shader

	vertex_shader_c   := strings.clone_to_cstring(vertex_shader_source, context.temp_allocator)
	fragment_shader_c := strings.clone_to_cstring(fragment_shader_source, context.temp_allocator)

	program_id = gl.CreateProgram()

	vertex_shader_id := gl.CreateShader(gl.VERTEX_SHADER)
	gl.ShaderSource(vertex_shader_id, 1, &vertex_shader_c, nil)
	gl.CompileShader(vertex_shader_id)
	when ODIN_DEBUG {
		ok = check_shader_compilation("vertex shader", vertex_shader_id)
		if !ok {
			return
		}
	}

	fragment_shader_id := gl.CreateShader(gl.FRAGMENT_SHADER)
	gl.ShaderSource(fragment_shader_id, 1, &fragment_shader_c, nil)
	gl.CompileShader(fragment_shader_id)
	when ODIN_DEBUG {
		ok = check_shader_compilation("fragment shader", fragment_shader_id)
		if !ok {
			return
		}
	}

	gl.AttachShader(program_id, vertex_shader_id)
	gl.AttachShader(program_id, fragment_shader_id)
	gl.LinkProgram(program_id)

	gl.DeleteShader(vertex_shader_id)
	gl.DeleteShader(fragment_shader_id)

	ok = init_shader_uniforms(&shader)
	if !ok {
		return
	}
	ok = init_shader_attributes(&shader)
	if !ok {
		return
	}
	shader_idx, ok = push_shader(shader)

	return
}

Quad_Vertex :: struct {
	position: v3,
	texture: v2,
}

QUAD_NUM_VERTICES :: 4

Quad :: struct {
	vertices: [QUAD_NUM_VERTICES]Quad_Vertex,
}

// TODO: split this into specific shaders for text rendering and other things
Quad_Shader :: struct {
	shader_id: int,

	// attributes
	position: u32 `gl_attrib:"position"`,
	texture:  u32 `gl_attrib:"texture"`,

	// uniforms
	position_offset: u32 `gl_uniform:"position_offset"`,
	ortho:           u32 `gl_uniform:"ortho"`,
	settings:        u32 `gl_uniform:"settings"`,
	font_texture:    u32 `gl_uniform:"fontTexture"`,
	color:           u32 `gl_uniform:"color"`,
	radius:          u32 `gl_uniform:"radius"`,
	dimensions:      u32 `gl_uniform:"dimensions"`,
	origin:          u32 `gl_uniform:"origin"`,

	// texture samplers
	font_texture_index: u32 `gl_sampler:"fontTexture"`
}

global_quad_shader := Quad_Shader{}

init_quad_shader :: proc(shader_idx: int) -> (ok: bool) {
	shader : Shader
	shader, ok = small_array.get_safe(global_shaders, shader_idx)
	assert(ok)
	if !ok {
		return
	}

	global_quad_shader.shader_id = shader_idx
	gl.UseProgram(shader.program_id)

	location       : u32
	attribute_name : string
	uniform_name   : string

	ti := runtime.type_info_base(type_info_of(Quad_Shader))
	if s, ok := ti.variant.(runtime.Type_Info_Struct); ok {
		num_fields := len(s.names)
		for i in 1..<num_fields {
			field_offset := s.offsets[i]
			location_field := cast(^u32)rawptr(uintptr(&global_quad_shader) + field_offset)
			if attribute_name, ok = reflect.struct_tag_lookup(reflect.Struct_Tag(s.tags[i]), "gl_attrib"); ok {
				location, ok = shader_attribute_location(shader, attribute_name)
				if !ok {
					log.errorf("Couldn't find location of attribute with name: {} in quad shader", attribute_name)
					return
				}
				location_field^ = location
			}
			if uniform_name, ok = reflect.struct_tag_lookup(reflect.Struct_Tag(s.tags[i]), "gl_uniform"); ok {
				location, ok = shader_uniform_location(shader, uniform_name)
				if !ok {
					log.errorf("Couldn't find location of uniform with name: {} in quad shader", uniform_name)
					return
				}
				location_field^ = location
			}
			if uniform_name, ok = reflect.struct_tag_lookup(reflect.Struct_Tag(s.tags[i]), "gl_sampler"); ok {
				location, ok = shader_uniform_location(shader, uniform_name)
				if !ok {
					log.errorf("Couldn't find location of uniform with name: {} in quad shader", uniform_name)
					return
				}
				uniform := shader.uniforms.data[location]
				assert(uniform.sampler_index >= 0)
				location_field^ = u32(uniform.sampler_index)
			}
		}
	}

	{
		using global_quad_shader

		gl.GenVertexArrays(1, &shader.vao)
		gl.BindVertexArray(shader.vao)

		gl.GenBuffers(1, &shader.vbo)
		gl.GenBuffers(1, &shader.ebo)

		stride := size_of(Quad_Vertex)

		gl.BindBuffer(gl.ARRAY_BUFFER, shader.vbo)
		gl.BufferData(gl.ARRAY_BUFFER, QUAD_NUM_VERTICES * stride, nil, gl.STATIC_DRAW)

		zero : uintptr = 0

		gl.EnableVertexAttribArray(position)
		gl.VertexAttribPointer(position, 3, gl.FLOAT, gl.FALSE, i32(stride), zero)
		gl.EnableVertexAttribArray(texture)
		// TODO: do we want to extract the size of these out and reflect on Quad_Vertex here?
		gl.VertexAttribPointer(texture, 2, gl.FLOAT, gl.FALSE, i32(stride), uintptr(size_of(v3)))

		quad_indices := [?]u32{
			0, 1, 2, 2, 3, 0,
		}
		gl.BindBuffer(gl.ELEMENT_ARRAY_BUFFER, shader.ebo)
		gl.BufferData(gl.ELEMENT_ARRAY_BUFFER, size_of(quad_indices), &quad_indices, gl.STATIC_DRAW)

		// TODO: macOS may break here if we do this before binding a VAO
		ok = check_shader_linking(shader.program_id)
		if !ok {
			return
		}
		ok = check_shader_program_valid(shader.program_id)
		if !ok {
			return
		}

		gl.BindVertexArray(0)
		gl.BindBuffer(gl.ARRAY_BUFFER, 0)
		gl.BindBuffer(gl.ELEMENT_ARRAY_BUFFER, 0)
	}

	ok = true
	return
}

Uniform_Data :: union {
	f32,
	u32,
	i32,
	v2,
	v3,
	v4,
}

Shader_Uniform :: struct {
	location: u32,
	type: Shader_Param_Type,
	data: Uniform_Data,
}

Texture_Type :: enum {
	Texture2D,
}

Shader_Texture :: struct {
	id:           u32,
	type:         Texture_Type,
	shader_index: u32,
}

MAX_SHADER_TEXTURES :: 8

Vertex_Element :: struct($N: int, $T: typeid) where N >= 0 {
	vertices: [N]T,
}

Shader_Call :: struct($N: int, $T: typeid) where N >= 0 {
	shader_id: int,
	uniforms: small_array.Small_Array(MAX_SHADER_UNIFORMS, Shader_Uniform),
	textures: small_array.Small_Array(MAX_SHADER_TEXTURES, Shader_Texture),
	vertices: small_array.Small_Array(N,                   T),
}

Quad_Vertex_Element :: Vertex_Element(4, Quad_Vertex)
Quad_Shader_Call    :: Shader_Call(1, Quad_Vertex_Element)
