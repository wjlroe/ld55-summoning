package main

// TODO: moving a window between monitors with different content scales
// Your code needs to handle the WM_WINDOWPOSCHANGED message in addition to the
// scale change event registered through RegisterScaleChangeEvent, because the app
// window can be moved between monitors. In response to the WM_WINDOWPOSCHANGED
// message, call MonitorFromWindow, followed by GetScaleFactorForMonitor to get the
// scale factor of the monitor which the app window is on. Your code can then react
// to any dots per inch (dpi) change by reloading assets and changing layout.

import "core:c"
import "core:fmt"
import gl "vendor:OpenGL"
import "base:intrinsics"
import "core:log"
import "core:math"
import "core:os"
import win32 "core:sys/windows"

win32_window : win32.HWND
win32_dc     : win32.HDC
win32_gl     : win32.HGLRC

last_error_message := [1024]u16{}

start_time_in_cycles : u64

get_current_counter :: proc() -> u64 {
	counter : win32.LARGE_INTEGER
	win32.QueryPerformanceCounter(&counter)
	return u64(counter)
}

get_time :: proc() -> f32 {
	elapsed := get_current_counter() - start_time_in_cycles
	freq : win32.LARGE_INTEGER
	win32.QueryPerformanceFrequency(&freq)
	return (f32(elapsed) / (1000.0 / f32(freq)))
}

win32_last_error :: proc() -> (error_message: string) {
	error := win32.GetLastError()

	win32.FormatMessageW(
		win32.FORMAT_MESSAGE_FROM_SYSTEM,
		nil,
		error,
		0,
		raw_data(last_error_message[:]),
		win32.DWORD(len(last_error_message)),
		nil,
	)

	message, alloc_err := win32.utf16_to_utf8(last_error_message[:])

	if alloc_err != nil {
		error_message = "Error allocating space for the error message!"
	} else {
		error_message = message
	}

	return
}

win32_load_opengl :: proc() -> (ok: bool) {
	window_class := win32.WNDCLASSW{
		lpfnWndProc = win32.DefWindowProcW,
		hInstance = cast(win32.HINSTANCE)win32.GetModuleHandleA(nil),
		style = win32.CS_HREDRAW | win32.CS_VREDRAW | win32.CS_OWNDC,
		lpszClassName = win32.utf8_to_wstring("Dummy_WSL_rita"),
	}
	if (win32.RegisterClassW(&window_class) == 0) {
		error_msg := win32_last_error()
		log.errorf("failed to register dummy window class for OpenGL: {}", error_msg)
		return false
	}
	dummy_window := win32.CreateWindowExW(
		0,
		window_class.lpszClassName,
		win32.utf8_to_wstring("Dummy OpenGL Window"),
		0,
		win32.CW_USEDEFAULT,
		win32.CW_USEDEFAULT,
		win32.CW_USEDEFAULT,
		win32.CW_USEDEFAULT,
		nil,
		nil,
		window_class.hInstance,
		nil
	)
	if (transmute(u64)dummy_window == 0) {
		error_msg := win32_last_error()
		log.errorf("failed to create dummy window for OpenGL: {}", error_msg)
		return false
	}
	dummy_dc := win32.GetDC(dummy_window)

	pixel_format_descriptor := win32.PIXELFORMATDESCRIPTOR{
		nSize = size_of(win32.PIXELFORMATDESCRIPTOR),
		nVersion = 1,
		iPixelType = win32.PFD_TYPE_RGBA,
		dwFlags = win32.PFD_DRAW_TO_WINDOW | win32.PFD_SUPPORT_OPENGL | win32.PFD_DOUBLEBUFFER,
		cColorBits = 32,
		cAlphaBits = 8,
		iLayerType = win32.PFD_MAIN_PLANE,
		cDepthBits = 24,
		cStencilBits = 8,
	}

	pixel_format := win32.ChoosePixelFormat(dummy_dc, &pixel_format_descriptor)
	if pixel_format == 0 {
		error_msg := win32_last_error()
		log.errorf("failed to find a suitable pixel format: {}", error_msg)
		return false
	}
	if !win32.SetPixelFormat(dummy_dc, pixel_format, &pixel_format_descriptor) {
		error_msg := win32_last_error()
		log.errorf("failed to set pixel format: {}", error_msg)
		return false
	}

	dummy_context := win32.wglCreateContext(dummy_dc)
	if dummy_context == nil {
		error_msg := win32_last_error()
		log.errorf("failed to create a dummy OpenGL rendering context: {}", error_msg)
		return false
	}

	if !win32.wglMakeCurrent(dummy_dc, dummy_context) {
		error_msg := win32_last_error()
		log.errorf("failed to activate dummy OpenGL rendering context: {}", error_msg)
		return false
	}

	win32.gl_set_proc_address(&win32.wglCreateContextAttribsARB, "wglCreateContextAttribsARB")
	win32.gl_set_proc_address(&win32.wglChoosePixelFormatARB, "wglChoosePixelFormatARB")

	win32.wglMakeCurrent(dummy_dc, nil)
	win32.wglDeleteContext(dummy_context)
	win32.ReleaseDC(dummy_window, dummy_dc)
	win32.DestroyWindow(dummy_window)

	return true
}

window_proc :: proc "std" (win32_window: win32.HWND, message: u32, wParam: win32.WPARAM, lParam: win32.LPARAM) -> win32.LRESULT {
	result : win32.LRESULT = 0
	switch (message) {
		// case win32.WM_CREATE:
		// 	// FIXME: supported since Windows 11 Build 22000 (how to detect?)
		// 	pref := dwm_winodw_corner_preference.round_small
		// 	win32.DwmSetWindowAttribute(win32_window, cast(u32)win32.DWMWINDOWATTRIBUTE.DWMWA_WINDOW_CORNER_PREFERENCE, &pref, size_of(pref))
			// result = win32.DefWindowProcW(win32_window, message, wParam, lParam)
		case win32.WM_CLOSE:
			win32.PostQuitMessage(0)
		// case win32.WM_SIZE:
		// 	window := win32_windows[win32_window]
		// 	if wParam == win32.SIZE_MINIMIZED {
		// 		window.minimized = true
		// 	} else {
		// 		window.minimized = false
		// 	}
		// case win32.WM_WINDOWPOSCHANGED:
		// 	window_pos := transmute(^win32.WINDOWPOS)lParam
		// 	window := win32_windows[win32_window]
		// 	new_position := v2s{int(window_pos.x), int(window_pos.y)}
		// 	if window.position != new_position {
		// 		window.position = new_position
		// 		window.window_moved = true
		// 		editor_state.dirty = true
		// 	}
		// 	new_window_dim := v2s{int(window_pos.cx), int(window_pos.cy)}
		// 	if window.window_dim != new_window_dim {
		// 		window.window_dim = new_window_dim
		// 		window.window_resized = true
		// 		editor_state.dirty = true

		// 		client_rect : win32.RECT
		// 		win32.GetClientRect(win32_window, &client_rect)
		// 		framebuffer_dim := v2s{int(client_rect.right), int(client_rect.bottom)}
		// 		window.framebuffer_dim = framebuffer_dim
		// 		window.renderer.framebuffer_dim = framebuffer_dim
		// 	}

		// 	// TODO: call GetWindowPlacement to get maximized state
		// 	// GetWindowPlacement
		// 	result = win32.DefWindowProcW(win32_window, message, wParam, lParam)
		// case win32.WM_WININICHANGE:
			// TODO: Post this as a message to the main thread or something
			// lparam_wstr := transmute(win32.wstring)lParam
			// lparam_str, err := win32.wstring_to_utf8(lparam_wstr, 1024)
			// log.infof("WININICHANGE: %s, is light theme active: %v", lparam_str, is_light_theme())
			// result = win32.DefWindowProcW(win32_window, message, wParam, lParam)
		case:
			result = win32.DefWindowProcW(win32_window, message, wParam, lParam)
	}
	return result
}

process_input :: proc() {
	message: win32.MSG

	for (cast(u64)win32.PeekMessageA(&message, nil, 0, 0, win32.PM_REMOVE) != 0) {
		switch message.message {
			case win32.WM_QUIT: {
				global_window.quit = true
			}
			// case win32.WM_KEYDOWN: {
			// 	make_key_down(int(message.wParam))
			// 	win32_translate_keyboard_input()
			// }
			// case win32.WM_KEYUP: {
			// 	make_key_up(int(message.wParam))
			// }
			// case win32.WM_CHAR: {
			// 	// wParam is UTF-16 codepoint
			// 	make_char_down(rune(message.wParam))
			// 	win32_translate_keyboard_input()
			// 	// FIXME: is this right?
			// 	make_char_up(rune(message.wParam))
			// }
			// case win32.WM_LBUTTONDOWN: {
			// 	point : win32.POINT
			// 	point.x = i32(message.lParam & 0xffff)
			// 	point.y = i32((message.lParam >> 16) & 0xffff)
			// 	input := Input{}
			// 	input.for_window = window_key
			// 	input.data = Mouse_Input{}
			// 	mouse_input := &input.data.(Mouse_Input)
			// 	mouse_input.position = v2{f32(point.x), f32(point.y)}
			// 	mouse_input.is_down = true
			// 	// TODO: mods
			// 	last_inputs[num_last_inputs] = input
			// 	num_last_inputs += 1
			// }
			case: {
				win32.TranslateMessage(&message)
				win32.DispatchMessageW(&message)
			}
		}
	}
}

win32_init_opengl :: proc(gl_major: i32, gl_minor: i32) -> (gl_context: win32.HGLRC, ok: bool) {
	GL_TRUE :: 1
	GL_FALSE :: 0

	pixel_format_attribs := []c.int{
		win32.WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
		win32.WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
		win32.WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
		win32.WGL_ACCELERATION_ARB,       win32.WGL_FULL_ACCELERATION_ARB,
		win32.WGL_PIXEL_TYPE_ARB,         win32.WGL_TYPE_RGBA_ARB,
		win32.WGL_COLOR_BITS_ARB,         32,
		win32.WGL_DEPTH_BITS_ARB,         24,
		win32.WGL_STENCIL_BITS_ARB,       8,
		0,
	}
	pixel_format : c.int
	num_formats : c.uint
	win32.wglChoosePixelFormatARB(win32_dc, &pixel_format_attribs[0], nil, 1, &pixel_format, &num_formats)
	if num_formats == 0 {
		log.errorf("Failed to find an OpenGL pixel format")
		return
	}

	pixel_format_descriptor : win32.PIXELFORMATDESCRIPTOR
	win32.DescribePixelFormat(win32_dc, pixel_format, size_of(pixel_format_descriptor), &pixel_format_descriptor)
	if !win32.SetPixelFormat(win32_dc, pixel_format, &pixel_format_descriptor) {
		log.errorf("Failed to set the OpenGL pixel format")
		return
	}

	assert(gl_major > 0, "Please set a non-zero OpenGL major version")
	assert(gl_minor > 0, "Please set a non-zero OpenGL minor version")
	gl_attribs := []c.int{
		win32.WGL_CONTEXT_MAJOR_VERSION_ARB, gl_major,
		win32.WGL_CONTEXT_MINOR_VERSION_ARB, gl_minor,
		win32.WGL_CONTEXT_LAYER_PLANE_ARB,   0, // main plane
		win32.WGL_CONTEXT_FLAGS_ARB,         win32.WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, // WGL_CONTEXT_DEBUG_BIT_ARB
		win32.WGL_CONTEXT_PROFILE_MASK_ARB,  win32.WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0,
	}

	gl_context = win32.wglCreateContextAttribsARB(win32_dc, nil, &gl_attribs[0])
	if gl_context == nil {
		log.infof("Failed to create context with OpenGL version: {}.{}", gl_major, gl_minor)
		return
	}

	if !win32.wglMakeCurrent(win32_dc, gl_context) {
		log.infof("Failed to make context current with OpenGL version: {}.{}", gl_major, gl_minor)
		return
	}

	ok = true
	return
}

init_window :: proc() -> (ok: bool) {
	start_time_in_cycles = get_current_counter()
	ok = win32_load_opengl()
	assert(ok)
	if !ok {
		log.error("Failed to load OpenGL!")
		return
	}
	instance := cast(win32.HINSTANCE)win32.GetModuleHandleA(nil)
	wide_title := win32.utf8_to_wstring(global_window.title)

	window_class := win32.WNDCLASSW{
		style = win32.CS_HREDRAW | win32.CS_VREDRAW | win32.CS_OWNDC,
		lpfnWndProc = window_proc,
		hInstance = instance,
		hCursor = win32.LoadCursorA(nil, win32.IDC_NO),
		hbrBackground = nil,
		lpszClassName = wide_title,
		lpszMenuName = wide_title,
	}
	if win32.RegisterClassW(&window_class) == 0 {
		error_msg := win32_last_error()
		log.errorf("failed to create win32 window_class: {}", error_msg)
		return
	}

	rect := win32.RECT{
		left = 0,
		top = 0,
		right = global_window.width,
		bottom = global_window.height,
	}
	window_style := win32.WS_OVERLAPPEDWINDOW
	win32.AdjustWindowRect(&rect, window_style, false)
	win32_window = win32.CreateWindowExW(
		0,
		window_class.lpszClassName,
		window_class.lpszMenuName,
		window_style,
		win32.CW_USEDEFAULT,
		0,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nil,
		nil,
		instance,
		nil,
	)
	if transmute(u64)win32_window == 0 {
		error_msg := win32_last_error()
		log.errorf("failed to create win32 window: {}", error_msg)
		return
	}

	win32_dc = win32.GetDC(win32_window)
	if win32_dc == nil {
		error_msg := win32_last_error()
		log.errorf("failed to get display context for win32 window: {}", error_msg)
		return
	}
	win32_gl, ok = win32_init_opengl(3, 3)
	assert(ok)

	gl.load_up_to(3, 3, win32.gl_set_proc_address)

	win32.ShowCursor(false)
	win32.ShowWindow(win32_window, win32.SW_SHOWNORMAL)
	win32.UpdateWindow(win32_window)
	ok = true
	return
}

swap_window :: proc() {
	win32.SwapBuffers(win32_dc)
}
