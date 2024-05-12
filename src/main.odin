package main

import rl "vendor:raylib"

DEFAULT_WINDOW_WIDTH  :: 1280
DEFAULT_WINDOW_HEIGHT :: 800

main :: proc() {
    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Ludum Dare 55: Summoning")

    for !rl.WindowShouldClose() {
        rl.BeginDrawing()
            rl.ClearBackground(rl.RAYWHITE)
            rl.DrawText("Congrats! You created your first window!", 190, 200, 20, rl.LIGHTGRAY)
        rl.EndDrawing()
    }

    rl.CloseWindow()
}
