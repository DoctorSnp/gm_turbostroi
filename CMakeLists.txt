
cmake_minimum_required (VERSION 3.15) #Проверка версии Cmake

project(gmsv_turbostroi VERSION 2.0.2 DESCRIPTION "gmsv_turbostroi description")
message ("Start building turbostroi") #Выводит сообщение "Start"
include(GNUInstallDirs)
add_library(gmsv_turbostroi SHARED src/gmsv_turbostroi.cpp)

link_directories("../bin-libs")
target_link_libraries (gmsv_turbostroi "lua5.1.5")

set_target_properties(gmsv_turbostroi PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32" )
set_target_properties(gmsv_turbostroi PROPERTIES  VERSION ${PROJECT_VERSION})

                        
add_definitions(-DGNUC -DPOSIX -D_LINUX )

# Папка с заголовочными файлами. Как можно понять. команда set 
# присваивает значение переменной.
set (CMAKE_INCLUDE_PATH include) 
set(CMAKE_SYSTEM_PROCESSOR "i686")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "c++ flags")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "c flags")

include_directories(
"./external/"
"./external/garrysmod/include/"
"./external/sourcesdk-minimal/game/shared/"
"./external/sourcesdk-minimal/public/shaderapi/"
"./external/sourcesdk-minimal/game/server"
"./external/sourcesdk-minimal/public/tier0/"
"./external/sourcesdk-minimal/public/"
"./external/sourcesdk-minimal/public/tier1/"
"./external/metamod-source/core/sourcehook/"
"./external/garrysmod/include/GarrysMod/Lua/",
"./external/luajit/"
"../include/luajit/"
    )

#set (PROJECT supertool)

if ( CMAKE_COMPILER_IS_GNUCXX ) 
    set (CMAKE_CXX_FLAGS "-Wall -fPIC -O2 -L/usr/local/lib") 
else ()
    message (FATAL_ERROR "I can't find G++!")
endif ()


