package main

import rl "vendor:raylib"

DEFAULT_WINDOW_WIDTH  :: 1280
DEFAULT_WINDOW_HEIGHT :: 800

WHITE          :: rl.Color{255, 255, 255, 255}
BLUE           :: rl.Color{10, 10, 255, 255}
RED            :: rl.Color{255, 10, 10, 255}
VERY_DARK_BLUE :: rl.Color{4, 8, 13, 255}
GREEN          :: rl.Color{10, 255, 10, 255}
AMBER          :: rl.Color{255, 191, 0, 255}

update_and_render :: proc() {
    rl.BeginDrawing()
    rl.ClearBackground(VERY_DARK_BLUE)
    rl.DrawText("Congrats! You created your first window!", 190, 200, 20, WHITE)
    rl.EndDrawing()
}

main :: proc() {
    rl.InitWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Ludum Dare 55: Summoning")

    for !rl.WindowShouldClose() {
        update_and_render()
    }

    rl.CloseWindow()
}
