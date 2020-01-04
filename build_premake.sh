#!/bin/bash

ROOT_DIR=$PWD

cd projects/linux;
make clean;
echo  "=== Clean after make ===";
rm -rf projects/linux/Makefile;
rm -rf projects/linux/gmsv_turbostroi.make;
rm -rf projects/linux/obj;
rm -rf build/*
echo "=== Exec Premake5 ==="
cd $ROOT_DIR
premake5 --os=linux gmake2

cd projects/linux
echo "=== Exec make -j5"
make -j5 config=release
#cd $ROOT_DIR

