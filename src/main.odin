package main

import rl "vendor:raylib"

main :: proc() {
    rl.InitWindow(800, 450, "raylib [core] example - basic window")

    for !rl.WindowShouldClose() {
        rl.BeginDrawing()
            rl.ClearBackground(rl.RAYWHITE)
            rl.DrawText("Congrats! You created your first window!", 190, 200, 20, rl.LIGHTGRAY)
        rl.EndDrawing()
    }

    rl.CloseWindow()
}
