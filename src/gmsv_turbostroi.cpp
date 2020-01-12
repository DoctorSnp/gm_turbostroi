#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque>
#include <sched.h> // для sched_set_affinity
#include "auxiliary.h"
#include "from_source_sdk.h"

#ifdef _WIN32
#include <SDKDDKVer.h> // Set the proper SDK version before including boost/Asio и Асио инклюдит windows.h на линуксе просто асио и все
#endif

#define _SCL_SECURE_NO_WARNINGS
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/atomic.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>

//  ============== EXPERIMENT 
//#include "external/metamod-source/sample_mm/engine_wrappers.h"
// ============================

#include "sourcehook_impl.h"

using namespace SourceHook;

//SourceSDK
//#undef _UNICODE

extern "C" {
	#include "external/lua.h"
	#include "external/luajit/luajit.h"
	#include "external/luajit/lualib.h"
	#include "external/luajit/lauxlib.h"
}


#ifdef _WIN32

int (WINAPIV * __vsnprintf)(char *, size_t, const char*, va_list) = _vsnprintf;
int (WINAPIV * __vsnwprintf)(wchar_t *, size_t, const wchar_t*, va_list) = _vsnwprintf;
#define strdup _strdup
#define wcsdup _wcsdup

#endif


#define GAME_DLL
#include "../game/server/cbase.h"
#undef GAME_DLL
#define _UNICODE

#define GARRYSMOD_LUA_SOURCECOMPAT_H
#include "GarrysMod/Lua/Interface.h"
using namespace GarrysMod::Lua;


#if defined(_MSC_VER)
    //  Microsoft 
    #define DLL_EXPORT extern "C" __declspec(dllexport)
    #define DLL_IMPORT extern "C" __declspec(dllimport)
#elif defined(__GNUC__)
    //  GCC
    #define DLL_EXPORT extern "C" __attribute__((visibility("default")))
    #define DLL_IMPORT extern "C"
#else
    //  do nothing and hope for the best?
    #define DLL_EXPORT
    #define DLL_IMPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif


//------------------------------------------------------------------------------
// SourceSDK
//------------------------------------------------------------------------------
SourceHook::Impl::CSourceHookImpl g_SourceHook;
SourceHook::ISourceHook *g_SHPtr = &g_SourceHook;
int g_PLID = 0;
CGlobalVars *g_GlobalVars = nullptr;
// чёто похожее
// CGlobalVars *gpGlobals;
// Эксперимент
IVEngineServer *engineServer = nullptr;
IServerGameDLL *engineServerDLL = nullptr;
IGameEventManager2 *gameEventManager = nullptr; // game events interface
IPlayerInfoManager *playerInfoManager = nullptr;
ICvar *g_pCVar = nullptr ;

//------------------------------------------------------------------------------
// Lua Utils
//------------------------------------------------------------------------------

static void stackDump(lua_State *L) {
	int i;
	int top = lua_gettop(L);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {

		case LUA_TSTRING:  /* strings */
			console_print(TURBO_COLOR_MAGENTA, "`%s'", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			console_print(TURBO_COLOR_MAGENTA "%d",  lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:  /* numbers */
			console_print(TURBO_COLOR_MAGENTA, "%d", lua_tonumber(L, i));
			break;

		default:  /* other values */
			console_print(TURBO_COLOR_MAGENTA, "%s", lua_typename(L, t));
			break;

		}
		console_print(TURBO_COLOR_MAGENTA, "  ");  /* put a separator */
	}
	console_print(TURBO_COLOR_MAGENTA, "\n");  /* end the listing */
}

//------------------------------------------------------------------------------
// Shared thread printing stuff
//------------------------------------------------------------------------------
#define BUFFER_SIZE 131072
#define QUEUE_SIZE 32768
double target_time = 0.0;
double rate = 100.0; //FPS
char metrostroiSystemsList[BUFFER_SIZE] = { 0 };
char loadSystemsList[BUFFER_SIZE] = { 0 };
#ifdef _WIN32
int SimThreadAffinityMask = 0;
#else
cpu_set_t  SimThreadAffinityMask;
#endif

std::map<int, IServerNetworkable*> trains_pos;
boost::unordered_map<std::string, std::string> load_files_cache;

typedef struct {
	int message;
	char system_name[64];
	char name[64];
	double index;
	double value;
} thread_msg;

struct thread_userdata {
	double current_time;
	lua_State* L;
	int finished;

	boost::lockfree::spsc_queue<thread_msg> thread_to_sim, sim_to_thread;

	thread_userdata() : thread_to_sim(1024), sim_to_thread(1024) //256
	{
	}
};

typedef struct {
	int ent_id;
	int id;
	char name[64];
	double value;
} rn_thread_msg;

struct rn_thread_userdata {
	double current_time;
	lua_State* L;
	int finished;

	boost::lockfree::spsc_queue<rn_thread_msg> thread_to_sim, sim_to_thread;

	rn_thread_userdata() : thread_to_sim(256), sim_to_thread(256) //256
	{
	}
};

struct shared_message {
	char message[512];
};

rn_thread_userdata* rn_userdata = NULL;
//------------------------------------------------------------------------------
// Turbostroi sim thread API
//------------------------------------------------------------------------------

boost::lockfree::queue <shared_message, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<64>> printMessages;
int shared_print(lua_State* L) {
	int n = lua_gettop(L);
	int i;
	char buffer[512];
	char* buf = buffer;
	buffer[0] = 0;

	lua_getglobal(L, "tostring");
	for (i = 1; i <= n; i++) {
		const char* str;
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);
		str = lua_tostring(L, -1);
		if (strlen(str) + strlen(buffer) < 512) {
			strcpy(buf, str);
			buf = buf + strlen(buf);
			buf[0] = '\t';
			buf = buf + 1;
			buf[0] = 0;
		}
		else if (i == 1 && buffer[0] == 0) {
			strcpy(buf, "[!] Message length limit reached!");
			buf = buf + strlen(buf);
			buf[0] = '\t';
			buf = buf + 1;
			buf[0] = 0;
		}
		lua_pop(L, 1);
	}
	buffer[strlen(buffer) - 1] = '\n';

	shared_message msg;
	char tempbuffer[512] = { 0 };
	strncat(tempbuffer, buffer, (512 - 1) - strlen(buffer));
	strcpy(msg.message, tempbuffer);
	printMessages.push(msg);
	return 0;
}

int thread_sendmessage_rpc(lua_State* state) {
	return 0;
}

DLL_EXPORT  bool ThreadSendMessage(void* p, int message, const char* system_name, const char* name, double index, double value) { //Нужно попробывать без extern "C"
	bool successful = false;

	thread_userdata* userdata = (thread_userdata*)p;

	if (userdata) {
		thread_msg tmsg;
		tmsg.message = message;
		strncpy(tmsg.system_name, system_name, 63);
		tmsg.system_name[63] = 0;
		strncpy(tmsg.name, name, 63);
		tmsg.name[63] = 0;
		tmsg.index = index;
		tmsg.value = value;
		if (userdata->thread_to_sim.push(tmsg)) {
			successful = true;
		}
	}
	return successful;
}

int thread_recvmessages(lua_State* state) {
	lua_getglobal(state,"_userdata");
	thread_userdata* userdata = (thread_userdata*)lua_touserdata(state,-1);
	lua_pop(state,1);

	if (userdata) {
		size_t total = userdata->sim_to_thread.read_available();
		lua_createtable(state, total, 0);
		for (size_t i = 0; i < total; ++i) {
			userdata->sim_to_thread.consume_one([&](thread_msg tmsg) {
				lua_createtable(state, 0, 5);
				lua_pushnumber(state, tmsg.message);     lua_rawseti(state, -2, 1);
				lua_pushstring(state, tmsg.system_name); lua_rawseti(state, -2, 2);
				lua_pushstring(state, tmsg.name);        lua_rawseti(state, -2, 3);
				lua_pushnumber(state, tmsg.index);       lua_rawseti(state, -2, 4);
				lua_pushnumber(state, tmsg.value);       lua_rawseti(state, -2, 5);
				lua_rawseti(state, -2, i);
				});
		}
		return 1;
	}
	return 0;
}

DLL_EXPORT  thread_msg ThreadRecvMessage(void* p) {
	thread_userdata* userdata = (thread_userdata*)p;
	thread_msg tmsg;
	if (userdata) {
		userdata->sim_to_thread.pop(tmsg);
	}
	return tmsg;
}

DLL_EXPORT  int ThreadReadAvailable(void* p) {
	thread_userdata* userdata = (thread_userdata*)p;
	return userdata->sim_to_thread.read_available();
}

//------------------------------------------------------------------------------
// RailNetwork sim thread API
//------------------------------------------------------------------------------

DLL_EXPORT bool RnThreadSendMessage(int ent_id, int id, const char* name, double value) {
	bool successful = true;

	if (rn_userdata) {
		rn_thread_msg tmsg;
		tmsg.ent_id = ent_id;
		tmsg.id = id;
		strncpy(tmsg.name, name, 63);
		tmsg.name[63] = 0;
		tmsg.value = value;
		if (!rn_userdata->thread_to_sim.push(tmsg)) {
			successful = false;
		}
	}
	else {
		successful = false;
	}
	return successful;
}

int thread_rnrecvmessages(lua_State* state) {
        console_print_debug(TURBO_COLOR_CYAN ," thread_rnrecvmessages\n");
	if (rn_userdata) {
		size_t total = rn_userdata->sim_to_thread.read_available();
		lua_createtable(state, total, 0);
		for (size_t i = 0; i < total; ++i) {
			rn_userdata->sim_to_thread.consume_one([&](rn_thread_msg tmsg) {
				lua_createtable(state, 0, 4);
				lua_pushnumber(state, tmsg.ent_id);					lua_rawseti(state, -2, 1);
				lua_pushnumber(state, tmsg.id);						lua_rawseti(state, -2, 2);
				lua_pushstring(state, tmsg.name);					lua_rawseti(state, -2, 3);
				lua_pushnumber(state, tmsg.value);					lua_rawseti(state, -2, 4);
				lua_rawseti(state, -2, i);
				});
		}
		return 1;
	}
	return 0;
}

// --- v2 Turbostroi Logic
void threadSimulation(thread_userdata* userdata) {
        console_print_debug(TURBO_COLOR_CYAN ,"threadSimulation\n");
	lua_State* L = userdata->L;
	boost::chrono::process_real_cpu_clock::time_point p = boost::chrono::time_point_cast<boost::chrono::milliseconds>(boost::chrono::process_real_cpu_clock::now());

	while (userdata && !userdata->finished) {
		lua_settop(L,0);

		//Simulate one step
		if (userdata->current_time < target_time) {
			userdata->current_time = target_time;
			lua_pushnumber(L, ElapsedTime ());
			lua_setglobal(L, "CurrentTime");

			//Execute think
			lua_getglobal(L,"Think");
			lua_pushboolean(L, false);
			if (lua_pcall(L, 1, 0, 0)) {
				std::string err = lua_tostring(L, -1);
				err += "\n";
				shared_message msg;
				strcpy(msg.message, err.c_str());
				printMessages.push(msg);
			}
		}
		else {
			//Execute think
			lua_pushnumber(L, ElapsedTime ());
			lua_setglobal(L, "CurrentTime");

			lua_getglobal(L, "Think");
			lua_pushboolean(L, true);
			if (lua_pcall(L, 1, 0, 0)) {
				std::string err = lua_tostring(L, -1);
				err += "\n";
				shared_message msg;
				strcpy(msg.message, err.c_str());
				printMessages.push(msg);
			}
		}

		boost::this_thread::sleep_for(boost::chrono::milliseconds((int)(1000 / rate)));
	}

	//Release resources
	lua_pushcfunction(L,shared_print);
	lua_pushstring(L,"[!] Terminating train thread");
	lua_call(L,1,0);
	lua_close(L);
	free(userdata);
}

void threadRailnetworkSimulation(rn_thread_userdata* userdata) {
	console_print_debug(TURBO_COLOR_CYAN ,"threadRailnetworkSimulation\n");
        lua_State* L = userdata->L;
	boost::chrono::process_real_cpu_clock::time_point p = boost::chrono::process_real_cpu_clock::now();

	while (userdata && !userdata->finished) {
		lua_settop(L,0);

		//Simulate one step
		if (userdata->current_time < target_time) {
			userdata->current_time = target_time;
			lua_pushnumber(L, userdata->current_time);
			lua_setglobal(L,"CurrentTime");
			
			lua_newtable(L);
			//for each (auto var in trains_pos)
                        for (auto var: trains_pos)
			{
				lua_createtable(L, 0, 3);
				float* pos = var.second->GetPVSInfo()->m_vCenter;
				lua_pushnumber(L, pos[0]);			lua_rawseti(L, -2, 1);
				lua_pushnumber(L, pos[1]);			lua_rawseti(L, -2, 2);
				lua_pushnumber(L, pos[2]);			lua_rawseti(L, -2, 3);
				lua_rawseti(L, -2, var.first);
			}
			lua_setglobal(L, "TrainsPos");

			//Execute think
			lua_getglobal(L,"Think");
			if (lua_pcall(L,0,0,0)) {
				std::string err = lua_tostring(L, -1);
				err += "\n";
				shared_message msg;
				strcpy(msg.message, err.c_str());
				printMessages.push(msg);
			}
		}
		p += boost::chrono::milliseconds((int)(1000 / rate));
		boost::this_thread::sleep_until(p);
	}

	//Release resources
	lua_pushcfunction(L,shared_print);
	lua_pushstring(L,"[!] Terminating RailNetwork thread");
	lua_call(L,1,0);
	lua_close(L);
	free(userdata);
}


//------------------------------------------------------------------------------
// Metrostroi Lua API
//------------------------------------------------------------------------------
void load(GarrysMod::Lua::ILuaBase* LUA, lua_State* L, char* filename, char* path, char* variable = NULL, char* defpath = NULL, bool json = false) {
    //console_print_debug(TURBO_COLOR_CYAN ,"void load\n");
	//Load up "sv_turbostroi.lua" in the new JIT environment
	const char* file_data = NULL;
	auto cache_item = load_files_cache.find(filename);
	if (cache_item != load_files_cache.end()) {
		file_data = cache_item->second.c_str();
	}
	else {
		LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->GetField(-1, "file");
		LUA->GetField(-1, "Read");
		LUA->PushString(filename);
		LUA->PushString(path);
		LUA->Call(2, 1); //file.Read(...)
		if (LUA->GetString(-1)) {
			file_data = LUA->GetString(-1);
			load_files_cache.emplace(filename, file_data);
		}
		LUA->Pop(); //returned result
		LUA->Pop(); //file
		LUA->Pop(); //GLOB
	}

	if (file_data) {
		if (!variable) {
			if (luaL_loadbuffer(L, file_data, strlen(file_data), filename) || lua_pcall(L, 0, LUA_MULTRET, 0)) {
				char buf[8192] = { 0 };
				LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
				LUA->GetField(-1, "MsgC");
				LUA->GetField(-2, "Color");
				LUA->PushNumber(255);
				LUA->PushNumber(0);
				LUA->PushNumber(127);
				LUA->Call(3, 1);
				LUA->PushString(lua_tostring(L, -1));
				LUA->Call(2, 0);
				LUA->Pop(); //GLOB
				lua_pop(L, 1);
			}
		} else {
			if (!json) {
				lua_pushstring(L, file_data);
				lua_setglobal(L, variable);
			}
			else {
				LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
				LUA->GetField(-1, "table");
				LUA->GetField(-1, "ToString");
				LUA->Remove(-2);
				LUA->GetField(-2, "table");
				LUA->GetField(-1, "Sanitise");
				LUA->Remove(-2);
				LUA->GetField(-3, "util");
				LUA->GetField(-1, "JSONToTable");
				LUA->Remove(-2);
				LUA->PushString(file_data);
				LUA->Call(1, 1);
				LUA->Call(1, 1);
				LUA->PushString(variable);
				LUA->PushBool(false);
				LUA->Call(3, 1);

				if (luaL_loadbuffer(L, LUA->GetString(-1), strlen(LUA->GetString(-1)), filename) || lua_pcall(L, 0, LUA_MULTRET, 0)) {
					char buf[8192] = { 0 };
					LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
					LUA->GetField(-1, "MsgC");
					LUA->GetField(-2, "Color");
					LUA->PushNumber(255);
					LUA->PushNumber(0);
					LUA->PushNumber(127);
					LUA->Call(3, 1);
					LUA->PushString(lua_tostring(L, -1));
					LUA->Call(2, 0);
					LUA->Pop(); //GLOB
					lua_pop(L, 1);
				}

				LUA->Pop(); //returned result
				LUA->Pop(); //GLOB
			}
		}
	} else {
		char Message[512] = { 0 };
		if (!defpath) {
			std::sprintf(Message, "Turbostroi: File not found! ('%s' in '%s' path)\n", filename, path);
		}
		else {
			std::sprintf(Message, "Turbostroi: File not found! ('%s' in '%s' path) Trying to load default file in '%s' path\n", filename, path, defpath);
		}
		LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->GetField(-1,"MsgC");
		LUA->GetField(-2,"Color");
		LUA->PushNumber(255);
		LUA->PushNumber(0);
		LUA->PushNumber(127);
		LUA->Call(3,1);
		LUA->PushString(Message);
		LUA->Call(2,0);
		LUA->Pop(); //GLOB

		if (defpath) {
			load(LUA, L, filename, defpath, variable, NULL, json);
		}
	}
}

LUA_FUNCTION( API_InitializeTrain ) 
{
	console_print_debug(TURBO_COLOR_CYAN ,"API_InitializeTrain\n");
       
    CBaseHandle* EntHandle;
	IServerNetworkable* Entity; 
        
	//Get entity index
	EntHandle = (CBaseHandle*)LUA->GetUserType<CBaseHandle>(1, Type::ENTITY);
         
    int index = EntHandle->GetEntryIndex();
    console_print_debug(TURBO_COLOR_YELLOW, "============ Debug 0.1 index is: %d\n", index);
    if( engineServer == nullptr)
    {
        console_print(TURBO_COLOR_RED, "Achtung!!!!! 'engineServer' is nullptr object!\n");
    }
    edict_t* EntityEdict = engineServer->PEntityOfEntIndex(index);
    if (EntityEdict == nullptr)
    {
        console_print(TURBO_COLOR_RED, "NULL 'EntityEdict' object!\n");
    }
    console_print_debug(TURBO_COLOR_YELLOW, "============ Debug 1 ============\n");
        // здесь крашится
    if (EntityEdict != nullptr)
        Entity = EntityEdict->GetNetworkable();
    console_print_debug(TURBO_COLOR_YELLOW, "============ Debug 2 ============\n");

    //if (EntityEdict != nullptr)
    trains_pos.insert(std::pair<int, IServerNetworkable*>(EntHandle->GetEntryIndex(), Entity));

    //Get current time
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1, "CurTime");
	LUA->Call(0, 1);
	double curtime = LUA->GetNumber(-1);
	LUA->Pop(2); //Curtime, GLOB

	//Initialize LuaJIT for train
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_bit(L);
	lua_pushboolean(L, 1);
	lua_setglobal(L, "TURBOSTROI");
	lua_pushcfunction(L, shared_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, thread_recvmessages);
	lua_setglobal(L, "RecvMessages");
        
	//Load neccessary files
	load(LUA, L, "metrostroi/sv_turbostroi_v2.lua", "LUA");
	load(LUA, L, "metrostroi/sh_failsim.lua", "LUA");

	//Load up all the systems
	char* systems = metrostroiSystemsList;
	while (systems[0]) {
		char filename[8192] = { 0 };
		char* system_name = systems;
		char* system_path = strchr(systems, '\n') + 1;
                
		strncpy(filename, system_path, strchr(system_path, '\n') - system_path);
        //console_print_debug(TURBO_COLOR_CYAN, "finding_in file name '%s'\n", filename);
        load(LUA, L, filename, "LUA");
		systems = system_path + strlen(filename) + 1;
	}
	//Initialize all the systems reported by the train
	systems = loadSystemsList;
	while (systems[0]) {
		char* system_name = systems;
		char* system_type = strchr(systems, '\n') + 1;
		*(strchr(system_name, '\n')) = 0;
		*(strchr(system_type, '\n')) = 0;

		lua_getglobal(L, "LoadSystems");
		lua_pushstring(L, system_type);
		lua_setfield(L, -2, system_name);

		systems = system_type + strlen(system_type) + 1;
	}
	loadSystemsList[0] = 0;
    console_print_debug(TURBO_COLOR_YELLOW, "============ Exit from WHILE LOOP ============\n");
	//Initialize systems
	lua_getglobal(L, "Initialize");
	if (lua_pcall(L, 0, 0, 0)) {
		lua_pushcfunction(L, shared_print);
		lua_pushvalue(L, -2);
		lua_call(L, 1, 0);
		lua_pop(L, 1);
	}

	//New userdata
	thread_userdata* userdata = new thread_userdata();
	userdata->finished = 0;
	userdata->L = L;
	userdata->current_time = curtime;
	LUA->PushUserType(userdata, 1);
	LUA->SetField(1, "_sim_userdata");
	lua_pushlightuserdata(L, userdata);
	lua_setglobal(L, "_userdata");
	//Create thread for simulation
	boost::thread thread(threadSimulation, userdata);
	
#ifdef _WIN32
        if (SimThreadAffinityMask)
            if (!SetThreadAffinityMask(thread.native_handle(), static_cast<uintptr_t>(SimThreadAffinityMask)))
                ConColorMsg(Color(255,0,0), "Turbostroi: SetSTAffinityMask failed on train thread! \n");
#else
        if (pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t),  &SimThreadAffinityMask) != 0 )
            console_print(TURBO_COLOR_RED,"Turbostroi: SetSTAffinityMask failed on train thread! \n");
#endif
        console_print(TURBO_COLOR_YELLOW, "============EXIT FROM API_InitializeTrain \n");
	return 0;
}

LUA_FUNCTION( API_DeinitializeTrain ) 
{
	//RailNetwork
        console_print_debug(TURBO_COLOR_CYAN ," API_DeinitializeTrain\n");
	CBaseHandle* EntHandle;
	//Get entity index
	EntHandle = (CBaseHandle*)LUA->GetUserType<CBaseHandle>(1, Type::ENTITY);
	trains_pos.erase(EntHandle->GetEntryIndex());
	//End RailNetwork

	LUA->GetField(1,"_sim_userdata");
	thread_userdata* userdata = LUA->GetUserType<thread_userdata>(-1, 1);
	if (userdata) userdata->finished = 1;
	LUA->Pop();

	LUA->PushNil();
	LUA->SetField(1,"_sim_userdata");
	
	return 0;
}

int API_InitializeRailnetwork(ILuaBase* LUA) {
	
        console_print_debug(TURBO_COLOR_CYAN ,"API_InitializeRailnetwork \n");
        char path_track[2048] = { 0 };
	char path_signs[2048] = { 0 };
	char path_sched[2048] = { 0 };

	//Get current time
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1, "CurTime");
	LUA->Call(0, 1);
	double curtime = LUA->GetNumber(-1);
	LUA->Pop(); //Curtime

	if (g_GlobalVars) {
		std::sprintf(path_track, "metrostroi_data/track_%s.txt", g_GlobalVars->mapname.ToCStr());
		std::sprintf(path_signs, "metrostroi_data/signs_%s.txt", g_GlobalVars->mapname.ToCStr());
		std::sprintf(path_sched, "metrostroi_data/sched_%s.txt", g_GlobalVars->mapname.ToCStr());
	}
	else {
		LUA->GetField(-1, "game");
		LUA->GetField(-1, "GetMap");
		LUA->Call(0, 1); //file.Read(...)
		const char* mapname = LUA->GetString(-1);
		LUA->Pop(2); //Read, GLOB

		std::sprintf(path_track, "metrostroi_data/track_%s.txt", mapname);
		std::sprintf(path_signs, "metrostroi_data/signs_%s.txt", mapname);
		std::sprintf(path_sched, "metrostroi_data/sched_%s.txt", mapname);
	}

	//Initialize LuaJIT for railnetwork
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_bit(L);
	lua_pushboolean(L, 1);
	lua_setglobal(L, "TURBOSTROI");
	lua_pushcfunction(L, shared_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, thread_rnrecvmessages);
	lua_setglobal(L, "RnRecvMessages");

	load(LUA, L, path_track, "DATA", "TrackLoadedData", "LUA", true); // Кажется крашит если нет таких файлов!
	load(LUA, L, path_signs, "DATA", "SignsLoadedData", "LUA", true);
	load(LUA, L, path_sched, "DATA", "SchedLoadedData", "LUA", true);
	load(LUA, L, "metrostroi/sv_turbostroi_railnetwork.lua", "LUA");
	//New userdata
	rn_thread_userdata* userdata = new rn_thread_userdata();
	userdata->finished = 0;
	userdata->L = L;
	userdata->current_time = curtime;
	rn_userdata = userdata;

	//Create thread for simulation
	boost::thread thread(threadRailnetworkSimulation, userdata);
#ifdef _WIN32
        if (SimThreadAffinityMask) {
		if (!SetThreadAffinityMask(thread.native_handle(), static_cast<uintptr_t>(SimThreadAffinityMask))) {
			console_print(TURBO_COLOR_RED, "Turbostroi: SetSTAffinityMask failed on rail network thread! \n");
		}
	}
#else
        // если хоть один CPU в родственном множестве
        for (int j = 0; j <boost::thread::hardware_concurrency() ; j++)
               if (CPU_ISSET(j, &SimThreadAffinityMask))
               {
                    if (pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t),  &SimThreadAffinityMask) != 0 )
                            console_print(TURBO_COLOR_RED,"Turbostroi: SetSTAffinityMask failed on train thread! \n");
                    return 0;
               }
#endif
	return 0;
}

int API_DeinitializeRailnetwork(ILuaBase* LUA)
{
	if (rn_userdata)
            rn_userdata->finished = 1;

	return 0;
}

LUA_FUNCTION( API_LoadSystem ) 
{
	console_print(TURBO_COLOR_CYAN ,"API_LoadSystem \n");
        if (LUA->GetString(1) && LUA->GetString(2)) {
		strncat(loadSystemsList,LUA->GetString(1),131071);
		strncat(loadSystemsList,"\n",131071);
		strncat(loadSystemsList,LUA->GetString(2),131071);
		strncat(loadSystemsList,"\n",131071);
	}
	return 0;
}

LUA_FUNCTION( API_RegisterSystem ) 
{
	if (LUA->GetString(1) && LUA->GetString(2)) {
		char msg[8192] = { 0 };
		LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->GetField(-1,"Msg");
		std::sprintf(msg,"Metrostroi: Registering system %s [%s]\n",LUA->GetString(1),LUA->GetString(2));
		LUA->PushString(msg);
		LUA->Call(1,0);
		LUA->Pop();

	
		strncat(metrostroiSystemsList,LUA->GetString(1),131071);
		strncat(metrostroiSystemsList,"\n",131071);
		strncat(metrostroiSystemsList,LUA->GetString(2),131071);
		strncat(metrostroiSystemsList,"\n",131071);
	}
	return 0;
}

LUA_FUNCTION( API_SendMessage ) 
{
	bool successful = true;
	LUA->CheckType(2,Type::NUMBER);
	LUA->CheckType(3,Type::STRING);
	LUA->CheckType(4,Type::STRING);
	LUA->CheckType(5,Type::NUMBER);
	LUA->CheckType(6,Type::NUMBER);

	LUA->GetField(1,"_sim_userdata");
	thread_userdata* userdata = LUA->GetUserType<thread_userdata>(-1, 1);
	LUA->Pop();

	if (userdata) {
		thread_msg tmsg;
		tmsg.message = (int)LUA->GetNumber(2);
		strncpy(tmsg.system_name,LUA->GetString(3),63);
		tmsg.system_name[63] = 0;
		strncpy(tmsg.name,LUA->GetString(4),63);
		tmsg.name[63] = 0;
		tmsg.index = LUA->GetNumber(5);
		tmsg.value = LUA->GetNumber(6);
		if (!userdata->sim_to_thread.push(tmsg)) {
			successful = false;
		}
	} else {
		successful = false;
	}
	LUA->PushBool(successful);
	return 1;
}

LUA_FUNCTION( API_RnSendMessage ) 
{
	bool successful = true;
	LUA->CheckType(2, Type::NUMBER);
	LUA->CheckType(3, Type::NUMBER);
	LUA->CheckType(4, Type::STRING);
	LUA->CheckType(6, Type::NUMBER);

	if (rn_userdata) {
		rn_thread_msg tmsg;
		tmsg.ent_id = LUA->GetNumber(2);
		tmsg.id = LUA->GetNumber(3);
		strncpy(tmsg.name, LUA->GetString(4), 63);
		tmsg.name[63] = 0;
		tmsg.value = LUA->GetNumber(5);
		if (!rn_userdata->sim_to_thread.push(tmsg)) {
			successful = false;
		}
	}
	else {
		successful = false;
	}
	LUA->PushBool(successful);
	return 1;
}

LUA_FUNCTION( API_RecvMessages ) 
{
	LUA->GetField(1,"_sim_userdata");
	thread_userdata* userdata = LUA->GetUserType<thread_userdata>(-1, 1);
	LUA->Pop();

	if (userdata) {
		LUA->CreateTable();
		for (size_t i = 0; i < userdata->thread_to_sim.read_available(); ++i) {
			userdata->thread_to_sim.consume_one([&](thread_msg tmsg) {
				LUA->PushNumber(i);
				LUA->CreateTable();
				LUA->PushNumber(1);		LUA->PushNumber(tmsg.message);			LUA->RawSet(-3);
				LUA->PushNumber(2);		LUA->PushString(tmsg.system_name);		LUA->RawSet(-3);
				LUA->PushNumber(3);		LUA->PushString(tmsg.name);				LUA->RawSet(-3);
				LUA->PushNumber(4);		LUA->PushNumber(tmsg.index);			LUA->RawSet(-3);
				LUA->PushNumber(5);		LUA->PushNumber(tmsg.value);			LUA->RawSet(-3);
				LUA->RawSet(-3);
				});
		}
		return 1;
	}
	return 0;
}

LUA_FUNCTION( API_RecvMessage ) 
{
	LUA->GetField(1, "_sim_userdata");
	thread_userdata* userdata = LUA->GetUserType<thread_userdata>(-1, 1);
	LUA->Pop();

	if (userdata) {
		thread_msg tmsg;
		if (userdata->thread_to_sim.pop(tmsg)) {
			LUA->PushNumber(tmsg.message);
			LUA->PushString(tmsg.system_name);
			LUA->PushString(tmsg.name);
			LUA->PushNumber(tmsg.index);
			LUA->PushNumber(tmsg.value);
			return 5;
		}
	}
	return 0;
}

LUA_FUNCTION( API_RnRecvMessages ) 
{
	if (rn_userdata) {
		LUA->CreateTable();
		for (size_t i = 0; i < rn_userdata->thread_to_sim.read_available(); ++i) {
			rn_userdata->thread_to_sim.consume_one([&](rn_thread_msg tmsg) {
			LUA->PushNumber(i);
			LUA->CreateTable();
			LUA->PushNumber(1);		LUA->PushNumber(tmsg.ent_id);				LUA->RawSet(-3);
			LUA->PushNumber(2);		LUA->PushNumber(tmsg.id);					LUA->RawSet(-3);
			LUA->PushNumber(3);		LUA->PushString(tmsg.name);					LUA->RawSet(-3);
			LUA->PushNumber(4);		LUA->PushNumber(tmsg.value);				LUA->RawSet(-3);
			LUA->RawSet(-3);
				});
		}
		return 1;
	}
	return 0;
}

LUA_FUNCTION( API_ReadAvailable ) 
{
	LUA->GetField(1, "_sim_userdata");
	thread_userdata* userdata = LUA->GetUserType<thread_userdata>(-1, 1);
	LUA->Pop();
	LUA->PushNumber(userdata->thread_to_sim.read_available());
	return 1;
}

LUA_FUNCTION( API_SetSimulationFPS ) 
{
	LUA->CheckType(1,Type::NUMBER);
	rate = LUA->GetNumber(1);
	return 0;
}

#ifdef _WIN32
LUA_FUNCTION( API_SetMTAffinityMask ) 
{
	LUA->CheckType(1, Type::NUMBER);
	int MTAffinityMask = (int)LUA->GetNumber(1);
	ConColorMsg(Color(0, 255, 0), "Turbostroi: Main Thread Running on CPU%i \n", GetCurrentProcessorNumber());
	if (!SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(MTAffinityMask))) {
		ConColorMsg(Color(255, 0, 0), "Turbostroi: SetMTAffinityMask failed! \n");
	}
	else {
		ConColorMsg(Color(0, 255, 0), "Turbostroi: Changed to CPU%i \n", GetCurrentProcessorNumber());
	}
	return 0;
}

LUA_FUNCTION( API_SetSTAffinityMask ) 
{
	LUA->CheckType(1, Type::NUMBER);
	SimThreadAffinityMask = (int)LUA->GetNumber(1);
	return 0;
}

#else // now only POSIX
LUA_FUNCTION( API_SetMTAffinityMask ) 
{
	LUA->CheckType(1, Type::NUMBER);
        cpu_set_t MTAffinityMask;
        
        CPU_ZERO(&MTAffinityMask);
        for (int j = 0; j < boost::thread::hardware_concurrency(); j++)
               CPU_SET(j, &MTAffinityMask);
        
    console_print(TURBO_COLOR_MAGENTA, "Turbostroi: Main Thread Running on CPU %d \n", sched_getcpu());
        
        for (int j = 0; j <boost::thread::hardware_concurrency() ; j++) {
               if (CPU_ISSET(j, &MTAffinityMask))
               {
                    if (pthread_setaffinity_np(pthread_self() , sizeof(cpu_set_t),  &MTAffinityMask) != 0 )
                            console_print(TURBO_COLOR_RED,"Turbostroi: SetMTAffinityMask failed ! \n");
                }
       		else 
                     console_print(TURBO_COLOR_MAGENTA, "Turbostroi: Changed to CPU%d \n", sched_getcpu() );
                
                return 0;
	}
	return 0;
}

LUA_FUNCTION( API_SetSTAffinityMask ) 
{
	LUA->CheckType(1, Type::NUMBER);
        CPU_ZERO(&SimThreadAffinityMask);
        for (int j = 0; j < boost::thread::hardware_concurrency(); j++)
               CPU_SET(j, &SimThreadAffinityMask);
	return 0;
}


#endif

LUA_FUNCTION( API_StartRailNetwork ) 
{
	if (rn_userdata) {
		API_DeinitializeRailnetwork( LUA );
	}
	API_InitializeRailnetwork( LUA );
	return 0;
}

//------------------------------------------------------------------------------
// Initialization SourceSDK
//------------------------------------------------------------------------------
SH_DECL_HOOK1_void(IServerGameDLL, Think, SH_NOATTRIB, 0, bool);
static void Think_handler(bool finalTick) {
	target_time = g_GlobalVars->curtime;
	shared_message msg;
	if (printMessages.pop(msg)) {
		console_print(TURBO_COLOR_MAGENTA, "%s\n", msg.message);
	}
}

void InstallHooks() {
	console_print(TURBO_COLOR_MAGENTA,  "Turbostroi: Installing hooks!\n");
	SH_ADD_HOOK_STATICFUNC(IServerGameDLL, Think, engineServerDLL, Think_handler, false);
}

void ClearLoadCache(const CCommand &command) {
	load_files_cache.clear();
	console_print(TURBO_COLOR_MAGENTA, "Turbostroi: Cache cleared!\n");
}

void InitInterfaces() {
    if (!(Sys_LoadInterface("engine", INTERFACEVERSION_VENGINESERVER, NULL, reinterpret_cast<void**>(&engineServer))))
	{ 
        console_print(TURBO_COLOR_RED, "Turbostroi: Unable to load Engine Interface!\n");
	}
	Sys_LoadInterface("server", INTERFACEVERSION_SERVERGAMEDLL, NULL, reinterpret_cast<void**>(&engineServerDLL));
	if (!engineServerDLL)
	{
        console_print(TURBO_COLOR_RED, "Turbostroi: Unable to load SGameDLL Interface!\n");
	}
	Sys_LoadInterface("server", INTERFACEVERSION_PLAYERINFOMANAGER, NULL, reinterpret_cast<void**>(&playerInfoManager));
	if (!playerInfoManager)
	{
        console_print(TURBO_COLOR_RED, "Turbostroi: Unable to load PlayerInfoManager Interface!\n");
	}
	Sys_LoadInterface("vstdlib", CVAR_INTERFACE_VERSION, NULL, reinterpret_cast<void**>(&g_pCVar));
	if (!g_pCVar)
	{
        console_print(TURBO_COLOR_RED, "Turbostroi: Unable to load CVAR Interface!\n");
	}

	if (playerInfoManager) g_GlobalVars = playerInfoManager->GetGlobalVars();
	InstallHooks();

	g_pCVar->RegisterConCommand(new ConCommand("turbostroi_clear_cache", ClearLoadCache, "Clear loaded files cache."));
}

void init()
{
#ifdef _WIN32
SimThreadAffinityMask = 0;
#else
CPU_ZERO(&SimThreadAffinityMask);
#endif
    
}

//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------
GMOD_MODULE_OPEN() {
	init();
    InitInterfaces();

	//Check whether being ran on server
	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
	LUA->GetField(-1,"SERVER");
	if (LUA->IsType(-1,Type::NIL)) {
		LUA->GetField(-2,"Msg");
		LUA->PushString("Metrostroi: DLL failed to initialize (gm_turbostroi.dll can only be used on server)\n");
		LUA->Call(1,0);
		return 0;
	}
	LUA->Pop(); //SERVER

	//Check for global table
	LUA->GetField(-1,"Metrostroi");
	if (LUA->IsType(-1,Type::NIL)) {
		LUA->GetField(-2,"Msg");
		LUA->PushString("Metrostroi: DLL failed to initialize (cannot be used standalone without metrostroi addon)\n");
		LUA->Call(1,0);
		return 0;
	}
	LUA->Pop(); //Metrostroi

	//Initialize API
	LUA->CreateTable();
	LUA->PushCFunction(API_InitializeTrain);
	LUA->SetField(-2,"InitializeTrain");
	LUA->PushCFunction(API_DeinitializeTrain);
	LUA->SetField(-2,"DeinitializeTrain");
	LUA->PushCFunction(API_StartRailNetwork);
	LUA->SetField(-2,"StartRailNetwork");
	//LUA->PushCFunction(API_Think); // depricated. using engine think hook
	//LUA->SetField(-2,"Think");
	LUA->PushCFunction(API_SendMessage);
	LUA->SetField(-2,"SendMessage");
	LUA->PushCFunction(API_RnSendMessage);
	LUA->SetField(-2, "RnSendMessage");

	LUA->PushCFunction(API_RecvMessages);
	LUA->SetField(-2,"RecvMessages");
	LUA->PushCFunction(API_RecvMessage);
	LUA->SetField(-2, "RecvMessage");
	LUA->PushCFunction(API_RnRecvMessages);
	LUA->SetField(-2, "RnRecvMessages");

	LUA->PushCFunction(API_LoadSystem);
	LUA->SetField(-2,"LoadSystem");
	LUA->PushCFunction(API_RegisterSystem);
	LUA->SetField(-2,"RegisterSystem");

	LUA->PushCFunction(API_ReadAvailable);
	LUA->SetField(-2, "ReadAvailable");
	LUA->PushCFunction(API_SetSimulationFPS);
	LUA->SetField(-2,"SetSimulationFPS");
	//LUA->PushCFunction(API_SetTargetTime); //deprecated. using engine think hook
	//LUA->SetField(-2,"SetTargetTime");
	LUA->PushCFunction(API_SetMTAffinityMask);
	LUA->SetField(-2, "SetMTAffinityMask");
	LUA->PushCFunction(API_SetSTAffinityMask);
	LUA->SetField(-2, "SetSTAffinityMask");

	LUA->SetField(-2,"Turbostroi");

	//Print some information
	LUA->GetField(-1,"Msg");
	LUA->Push(-1);

	LUA->PushString("Metrostroi: DLL initialized (built " __DATE__ ")\n");
	LUA->Call(1,0);

	char msg[1024] = { 0 };
	std::sprintf(msg,"Metrostroi: Running with %i cores\n", boost::thread::hardware_concurrency());
	LUA->PushString(msg);
	LUA->Call(1,0);
	LUA->Pop();

	if (!printMessages.is_lock_free()) {
		console_print(TURBO_COLOR_MAGENTA, "Turbostroi: Not fully supported! \n");
	}

	return 0;
}

//------------------------------------------------------------------------------
// Deinitialization
//------------------------------------------------------------------------------
GMOD_MODULE_CLOSE() {
	ConCommand* cmd = g_pCVar->FindCommand("turbostroi_clear_cache");
	if (cmd != nullptr) {
		g_pCVar->UnregisterConCommand(cmd);
	}
	return 0;
}
