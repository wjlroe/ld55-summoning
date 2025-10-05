# Ludum Dare 55: Summoning

* Tech: Odin + raylib
* IM Fell fonts from https://fonts.google.com/specimen/IM+Fell+English

## Setup

### Windows

Setup the symlink that VSCode will need to open the workspace file:

```
 cmd /c mklink /d odin-source C:\dev\odin\dev-master
 cmd /c mklink /d raylib-source C:\Users\will\Code\source\raylib
```

## WebASM docs

* https://blog.logrocket.com/first-game-in-webassembly/
* https://emscripten.org/docs/getting_started/downloads.html
* https://emscripten.org/docs/compiling/Building-Projects.html?highlight=sdl2#emscripten-ports <- wasm ports of C libraries (e.g. SDL2)
* https://github.com/emscripten-ports <- find ports for emscripten/wasm

## TODO

- [x] Load an image in native and web
- [x] Load and display a font in native and web
- [ ] Load and play an audio file in native and web
