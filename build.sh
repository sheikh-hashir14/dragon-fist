@echo off
cd /d "%~dp0"
if not exist dist mkdir dist
gcc src/main.c -Ilib/raylib/include -Llib/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -static -o dist/dragon-fist.exe
if errorlevel 1 (echo Build failed) else (echo Built dist\dragon-fist.exe)
pause