workspace "gmsv_turbostroi"
    configurations { "Debug", "Release" }
    location ( "projects/" .. os.target() )
       
    
project "gmsv_turbostroi"
	language "C++"
    kind     "SharedLib"
    prebuildmessage "message++"
    defines 
	{ 
		"GNUC", 
		"POSIX",
		"_LINUX"
	}
    
	files
    {
        "src/**.*"
    }

    --"external/metamod-source/core/sourcehook//**.cpp"
    --architecture "x64"
    architecture "x86"
    language     "C++"
    includedirs  { 
        "./external/",
		"./external/garrysmod/include/",
		"./external/sourcesdk-minimal/game/shared/",
		"./external/sourcesdk-minimal/public/shaderapi/",
		"./external/sourcesdk-minimal/game/server",
		"./external/sourcesdk-minimal/public/tier0/",
		"./external/sourcesdk-minimal/public/",
		"./external/sourcesdk-minimal/public/tier1/",
		"./external/metamod-source/core/sourcehook/",
		"./external/garrysmod/include/GarrysMod/Lua/", 
		"./external/luajit/",
        "../include/luajit/"
	}
    targetdir    "build"
    symbols      "On"
	buildoptions { "`wx-config --cxxflags`", "-ansi", "-fPIC" , " -std=c++11",
                   "-m32"  } 
    --buildoptions { "`wx-config --cflags`", "-ansi", "-fPIC" , "-std=c99" } 
    
	if os.istarget( "windows" ) then 
		targetsuffix "_win32" 
	end
    
	if os.istarget( "macosx" )  then 
		targetsuffix "_osx"   
	end
    
	if os.istarget( "linux" )   then 
        defines { "LINUX" }
        targetsuffix "_linux" 
    end

	staticruntime "On"

	filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
    
	configuration "Debug"
        optimize "Debug"

    configuration "Release"
        optimize "Speed"
        --flags    "StaticRuntime"

   -- configuration {}

    libdirs { "libs", "./bin-libs" }
	links { "lua5.1.5",
 	"boost_system", "boost_thread", "boost_timer",
 	"boost_atomic", "boost_chrono", "boost_date_time"
 } 
