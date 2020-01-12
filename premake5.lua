workspace "gmsv_turbostroi"
    configurations { "Debug", "Release" }
    location ( "projects/" .. os.get() )

project "gmsv_turbostroi"
    kind         "SharedLib"
    --architecture "x64"
    architecture "x86"
    language     "C++"
    includedirs  {
        "./etcexternal/",
        "./external/garrysmod/include/",
        "./external/sourcesdk-minimal/game/shared/",
        "./external/sourcesdk-minimal/game/server",
        "./external/sourcesdk-minimal/public/shaderapi/",
        "./external/sourcesdk-minimal/public/",
        "./external/sourcesdk-minimal/public/tier0/",
        "./external/sourcesdk-minimal/",
        "./external/sourcesdk-minimal/tier1/",
        "./external/sourcesdk-minimal/public/tier1/",
        "./external/metamod-source/core/sourcehook/",
        "./external/garrysmod/include/GarrysMod/Lua/",
        "./external/luajit/",
        "./external/"
	}
    targetdir    "build"
    symbols      "On"
    buildoptions { "`wx-config --cxxflags`", "-ansi", "-fPIC" , "-std=gnu++11"  } 
	--buildoptions { "`wx-config --cflags`", "-ansi", "-fPIC" , "-std=c99" } 
    linkoptions { "-ldl -lpthread -m32 -z defs" }
	defines { "NO_MALLOC_OVERRIDE", "GNUC" ,
			"POSIX", "_LINUX", "LINUX" }

	if os.is( "windows" ) then targetsuffix "_win32" end
    if os.is( "macosx" )  then targetsuffix "_osx"   end
    if os.is( "linux" )   then targetsuffix "_linux" end

    configuration "Debug"
        optimize "Debug"

    configuration "Release"
        optimize "Speed"
        flags    "StaticRuntime"

    configuration {}

    files
    {
        "src/**.*",
	    "external/metamod-source/core/sourcehook/*.h",
	    "external/metamod-source/core/sourcehook/*.cpp"
    }
	libdirs { "depends/boost_1_72_0/stage/lib/", "bin_libs" }
	links { "lua5.1.5" ,
		"boost_system", "boost_thread", "boost_chrono",
		"boost_date_time", "boost_atomic"
	} --"tier0",
