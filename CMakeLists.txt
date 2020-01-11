
cmake_minimum_required (VERSION 3.15) #Проверка версии Cmake

project(gmsv_turbostroi VERSION 2.0.2 DESCRIPTION "gmsv_turbostroi description")
message ("Start building ${PROJECT_NAME} ver ${PROJECT_VERSION}")

include(GNUInstallDirs)

find_package(Threads REQUIRED)

file( GLOB GLOB_LIB_SRC_LIST
    #"external/metamod-source/core/sourcehook/ *.h"
    "external/metamod-source/core/sourcehook/*.cpp"
    #"external/metamod-source/core/metamod.cpp"
    )

file( GLOB GLOB_SOURCESDK
    #"external/sourcesdk-minimal/game/shared/*.h"
    #"external/sourcesdk-minimal/game/shared/*.cpp"
    #"external/sourcesdk-minimal/game/server/*.h"
    #"external/sourcesdk-minimal/game/server/*.cpp"
    #"external/sourcesdk-minimal/public/shaderapi/*.h"
   # "external/sourcesdk-minimal/public/shaderapi/*.cpp"
    #"external/sourcesdk-minimal/public/tier0/*.h"
    #"external/sourcesdk-minimal/public/tier0/memoverride.cpp"
    
    #"external/sourcesdk-minimal/public/tier1/*.h"
    "external/sourcesdk-minimal/tier1/*.h"
    "external/sourcesdk-minimal/tier1/*.cpp"
    "external/sourcesdk-minimal/public/tier1/*.cpp"
)

set (LIB_SRC 
    src/gmsv_turbostroi.cpp
    src/auxiliary.cpp
	src/from_source_sdk.cpp
    ${GLOB_LIB_SRC_LIST}
    #${GLOB_SOURCESDK}
)

add_definitions(-DGNUC  -DPOSIX  -D_LINUX -DLINUX -DPROJECT_VER="${PROJECT_VERSION}" )

set(CMAKE_SYSTEM_PROCESSOR "i686")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -Wall -fPIC -O2 " CACHE STRING "c++ flags")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32 -Wall -fPIC -O2 " CACHE STRING "c flags")

add_library ( 
${PROJECT_NAME} SHARED  ${LIB_SRC} 
)

target_include_directories(
        ${PROJECT_NAME} PUBLIC 
        ${PROJECT_SOURCE_DIR}/external/
        ${PROJECT_SOURCE_DIR}/external/garrysmod/include/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/game/shared/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/game/server
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/public/shaderapi/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/public/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/public/tier0/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/tier1/
        ${PROJECT_SOURCE_DIR}/external/sourcesdk-minimal/public/tier1/
        ${PROJECT_SOURCE_DIR}/external/metamod-source/core/sourcehook/
        ${PROJECT_SOURCE_DIR}/external/garrysmod/include/GarrysMod/Lua/
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/depends/
        #${PROJECT_SOURCE_DIR}/external/luajit/
)

# требуем от gcc генерировать 32 битный код, даже на 64 битном линуксе
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-ldl -lpthread -m32 -z defs" ) #-z defs
set_target_properties(${PROJECT_NAME} PROPERTIES  VERSION ${PROJECT_VERSION})

target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/bin_libs/liblua5.1.5.a)
target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/depends/boost_1_72_0/stage/lib/libboost_system.a )
target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/depends/boost_1_72_0/stage/lib/libboost_thread.a )
target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/depends/boost_1_72_0/stage/lib/libboost_chrono.a )
target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/depends/boost_1_72_0/stage/lib/libboost_date_time.a )
target_link_libraries(${PROJECT_NAME}  PRIVATE ${PROJECT_SOURCE_DIR}/depends/boost_1_72_0/stage/lib/libboost_atomic.a )
