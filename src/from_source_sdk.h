#ifndef    __FROM_INTERFACE_H__
#define   __FROM_INTERFACE_H__

//********************************
#include "interface.h"
#include "eiface.h"
#include "Color.h"
#include "dbg.h"
#include "../game/server/iplayerinfo.h"
#include "iserver.h"
#include "convar.h"
#include "icvar.h"
#include "auxiliary.h"
//********************************

#if defined( _WIN32 ) || defined( WIN32 )
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#else	//_WIN32
#define PATHSEPARATOR(c) ((c) == '/')
#endif	//_WIN32

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR '\\'
#define INCORRECT_PATH_SEPARATOR '/'
#elif POSIX
#define CORRECT_PATH_SEPARATOR '/'
#define INCORRECT_PATH_SEPARATOR '\\'
#endif

CSysModule *Sys_LoadModule( const char *pModuleName, Sys_Flags flags /* = SYS_NOFLAGS (0) */ );
void Sys_UnloadModule( CSysModule *pModule );

HMODULE Sys_LoadLibrary( const char *pLibraryName, Sys_Flags flags );
const char * V_GetFileExtension( const char * path );
char *V_strncat(char *pDest, const char *pSrc, size_t destBufferSize, int max_chars_to_copy );
void V_SetExtension( char *path, const char *extension, int pathStringLength );
bool V_IsAbsolutePath ( const char *pStr );
bool Sys_LoadInterface(
	const char *pModuleName,
	const char *pInterfaceVersionName,
	CSysModule **pOutModule,
	void **pOutInterface );

CreateInterfaceFn Sys_GetFactory( CSysModule *pModule );

int V_snprintf( char *pDest, int maxLen, char const *pFormat, ... );
void V_StripExtension( const char *in, char *out, int outSize );
int	_V_strlen(const char* file, int line, const char *str);
void V_strncpy( char *pDest, char const *pSrc, int maxLen );
void V_FixSlashes( char *pname, char separator /* = CORRECT_PATH_SEPARATOR */ );
void *Sys_GetProcAddress( HMODULE hModule, const char *pName );

/*
class IMemAlloc //MyMemAlloc : public IMemAlloc
{
public:
	// Release versions
	static void *Alloc( size_t nSize ) override { return malloc(nSize); };
	static  void *Realloc( void *pMem, size_t nSize ) override   { return realloc(pMem, nSize); }
	static  void Free( void *pMem ) override { free(pMem) ; }
       // void *Expand_NoLongerSupported( void *pMem, size_t nSize ) { override ; }
};
*/

#endif // __FROM_INTERFACE_H__
