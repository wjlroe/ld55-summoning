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
import "core:log"
import "core:math"
import "core:os"
import win32 "core:sys/windows"

win32_window : win32.HWND

last_error_message := [1024]u16{}

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

window_proc :: proc "std" (win32_window: win32.HWND, message: u32, wParam: win32.WPARAM, lParam: win32.LPARAM) -> win32.LRESULT {
	result : win32.LRESULT = 0
	switch (message) {
		// case win32.WM_CREATE:
		// 	// FIXME: supported since Windows 11 Build 22000 (how to detect?)
		// 	pref := dwm_winodw_corner_preference.round_small
		// 	win32.DwmSetWindowAttribute(win32_window, cast(u32)win32.DWMWINDOWATTRIBUTE.DWMWA_WINDOW_CORNER_PREFERENCE, &pref, size_of(pref))
		// 	result = win32.DefWindowProcW(win32_window, message, wParam, lParam)
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

init_window :: proc() {
	fmt.println("win32 init_window")
	instance := cast(win32.HINSTANCE)win32.GetModuleHandleA(nil)
	wide_title := win32.utf8_to_wstring(global_window.title)

	window_class := win32.WNDCLASSW{
		style = win32.CS_HREDRAW | win32.CS_VREDRAW | win32.CS_OWNDC,
		lpfnWndProc = window_proc,
		hInstance = instance,
		hCursor = win32.LoadCursorA(nil, win32.IDC_ARROW),
		hbrBackground = nil,
		lpszClassName = wide_title,
		lpszMenuName = wide_title,
	}
	if win32.RegisterClassW(&window_class) == 0 {
		error_msg := win32_last_error()
		fmt.printf("failed to create win32 window_class: {}\n", error_msg)
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
		// alert_box("Failed to create window")

		error_msg := win32_last_error()
		fmt.printf("failed to create win32 window: {}\n", error_msg)
	}
}


