#!/bin/sh

TARGET="$HOME/.steam/steamcmd/server/garrysmod/lua/bin"
NAME="gmsv_turbostroi_linux.dll"

rm -rf build 2> /dev/null
mkdir build
cd build
cmake ..
make
rm -f "$TARGET/$NAME"
cp "$NAME" "$TARGET"
