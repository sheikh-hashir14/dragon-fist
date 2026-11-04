@echo off
cd /d "%~dp0"
if not exist "lib\raylib\include\raylib.h" (
  echo ERROR: raylib.h not found in %CD%\lib\raylib\include
  pause & exit /b 1
)
if not exist dist mkdir dist
gcc src/main.c -Ilib/raylib/include -Llib/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -static -o dist/dragon-fist.exe
if errorlevel 1 (
  echo Build failed
) else (
  xcopy /E /I /Y resources dist\resources >nul
  echo Built dist\dragon-fist.exe
)
pause