# Ludum Dare 55: Summoning

Started with base code from: https://git.sr.ht/~willroe/base-code-c-webasm
* IM Fell fonts from https://www.dafont.com/im-fell-types.font

Tech: C + SDL2 + WebASM

## WebASM docs

* https://blog.logrocket.com/first-game-in-webassembly/
* https://emscripten.org/docs/getting_started/downloads.html
* https://emscripten.org/docs/compiling/Building-Projects.html?highlight=sdl2#emscripten-ports <- wasm ports of C libraries (e.g. SDL2)
* https://github.com/emscripten-ports <- find ports for emscripten/wasm

## SDL2 docs

* https://blog.conan.io/2023/07/20/introduction-to-game-dev-with-sdl2.html
* https://wiki.libsdl.org/SDL2/MigrationGuide SDL 1.2->2 guide
* https://github.com/libsdl-org/SDL/blob/main/docs/README-dynapi.md <- about dynamic/static linking of SDL2

## TODO

- [x] Load an image in native and web
- [x] Load and display a font in native and web
- [ ] Load and play an audio file in native and web
