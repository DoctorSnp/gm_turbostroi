#!/bin/bash

g++ -std=gnu++11 -fPIC -Wall -fPIC  \
	-DPOSIX -D__PORT_LINUX_EXPERIMENT__  -D__GCC__\
	-I ./external/garrysmod/include/ \
	-I ./external/sourcesdk-minimal/game/shared/ \
	-I ./external/sourcesdk-minimal/public/shaderapi/ \
	-I ./external/sourcesdk-minimal/game/server \
	-I ./external/sourcesdk-minimal/public/tier0/ \
	-I ./external/sourcesdk-minimal/public/ \
	-I ./external/sourcesdk-minimal/public/tier1/ \
	-I ./external/metamod-source/core/sourcehook/ \
	-c gmsv_turbostroi/gmsv_turbostroi.cpp 		>& output.txt			
