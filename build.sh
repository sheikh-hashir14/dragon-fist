#!/bin/sh
cd "$(dirname "$0")" || exit 1
mkdir -p dist

gcc src/main.c \
  -Ilib/raylib-linux/include -Llib/raylib-linux/lib \
  -l:libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11 \
  -o dist/dragon-fist || { echo "Build failed"; exit 1; }

rm -rf dist/resources
cp -r resources dist/resources
echo "Built dist/dragon-fist"