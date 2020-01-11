#!/bin/sh

TARGET="$HOME/MetrostroiServerYar/steam_server/garrysmod/lua/bin"
NAME="libgmsv_turbostroi.so"
NAME_INSTALL="gmsv_turbostroi_linux.dll"

rm -rf build 2> /dev/null
mkdir build
cd build &&
cmake ../ &&
cmake --build . -- -j9 &&

rm -f "$TARGET/$NAME_INSTALL"
cp "$NAME" "$TARGET/$NAME_INSTALL"
