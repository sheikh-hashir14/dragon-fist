@echo off
gcc src/main.c -Ilib/raylib/include -Llib/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -static -o dist/dragon-fist.exe