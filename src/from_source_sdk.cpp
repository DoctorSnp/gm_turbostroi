#include "from_source_sdk.h"
//-----------------------------------------------------------------------------
// Purpose: get the interface for the specified module and version
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool Sys_LoadInterface(
    
        
	const char *pModuleName,
	const char *pInterfaceVersionName,
	CSysModule **pOutModule,
	void **pOutInterface )
{
    
	CSysModule *pMod = Sys_LoadModule( pModuleName );
	if ( !pMod ) {
                console_print(TURBO_COLOR_YELLOW, "Fuck 1\n");
		return false;
        }

	CreateInterfaceFn fn = Sys_GetFactory( pMod );
	if ( !fn )
	{      
                console_print(TURBO_COLOR_YELLOW, "Fuck 2\n");
		Sys_UnloadModule( pMod );
		return false;
	}

	*pOutInterface = fn( pInterfaceVersionName, NULL );
	if ( !( *pOutInterface ) )
	{
		console_print(TURBO_COLOR_YELLOW, "Fuck 3\n");
                Sys_UnloadModule( pMod );
		return false;
	}

	if ( pOutModule )
		*pOutModule = pMod;
        console_print(TURBO_COLOR_YELLOW, "Zer good!!!\n");
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : module - windows HMODULE from Sys_LoadModule() 
//			*pName - proc name
// Output : factory for this module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory( CSysModule *pModule )
{
	if ( !pModule )
		return NULL;

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);
#ifdef _WIN32
	return reinterpret_cast<CreateInterfaceFn>(GetProcAddress( hDLL, CREATEINTERFACE_PROCNAME ));
#elif defined(POSIX)
	// Linux gives this error:
	//../public/interface.cpp: In function `IBaseInterface *(*Sys_GetFactory
	//(CSysModule *)) (const char *, int *)':
	//../public/interface.cpp:154: ISO C++ forbids casting between
	//pointer-to-function and pointer-to-object
	//
	// so lets get around it :)
	return (CreateInterfaceFn)(GetProcAddress( (void *)hDLL, CREATEINTERFACE_PROCNAME ));
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule *Sys_LoadModule( const char *pModuleName, Sys_Flags flags /* = SYS_NOFLAGS (0) */ )
{
	// If using the Steam filesystem, either the DLL must be a minimum footprint
	// file in the depot (MFP) or a filesystem GetLocalCopy() call must be made
	// prior to the call to this routine.
	char szCwd[1024];
	HMODULE hDLL = NULL;

	if ( !Q_IsAbsolutePath( pModuleName ) )
	{
		// full path wasn't passed in, using the current working dir
		_getcwd( szCwd, sizeof( szCwd ) );
		if (szCwd[strlen(szCwd) - 1] == '/' || szCwd[strlen(szCwd) - 1] == '\\' )
		{
			szCwd[strlen(szCwd) - 1] = 0;
		}

		char szAbsoluteModuleName[1024];
		size_t cCwd = strlen( szCwd );
		if ( strstr( pModuleName, "bin/") == pModuleName || ( szCwd[ cCwd - 1 ] == 'n'  && szCwd[ cCwd - 2 ] == 'i' && szCwd[ cCwd - 3 ] == 'b' )  )
		{
			// don't make bin/bin path
			Q_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s", szCwd, pModuleName );			
		}
		else
		{
			Q_snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/bin/%s", szCwd, pModuleName );
		}
		hDLL = Sys_LoadLibrary( szAbsoluteModuleName, flags );
	}
        
	if ( !hDLL )
	{
                console_print(TURBO_COLOR_RED, "hDLL is false or null!\n" );
		// full path failed, let LoadLibrary() try to search the PATH now
		hDLL = Sys_LoadLibrary( pModuleName, flags );
#if defined( _DEBUG )
		if ( !hDLL )
		{
// So you can see what the error is in the debugger...
#if defined( _WIN32 ) && !defined( _X360 )
			char *lpMsgBuf;
			
			FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL 
			);

			LocalFree( (HLOCAL)lpMsgBuf );
#elif defined( _X360 )
			DWORD error = GetLastError();
			Msg( "Error(%d) - Failed to load %s:\n", error, pModuleName );
#else
			console_print(COLOR_RED, "Failed to load %s: %s\n", pModuleName, dlerror() );
#endif // _WIN32
		}
#endif // DEBUG
	}

#if !defined(LINUX)
	// If running in the debugger, assume debug binaries are okay, otherwise they must run with -allowdebug
	if ( Sys_GetProcAddress( hDLL, "BuiltDebug" ) )
	{
		if ( !IsX360() && hDLL && 
			 !CommandLine()->FindParm( "-allowdebug" ) && 
			 !Sys_IsDebuggerPresent() )
		{
			Error( "Module %s is a debug build\n", pModuleName );
		}

		DevWarning( "Module %s is a debug build\n", pModuleName );

		if ( !s_bRunningWithDebugModules )
		{
			s_bRunningWithDebugModules = true;
			
#if 0 //def IS_WINDOWS_PC
			char chMemoryName[ MAX_PATH ];
			DebugKernelMemoryObjectName( chMemoryName );
			
			(void) CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024, chMemoryName );
			// Created a shared memory kernel object specific to process id
			// Existence of this object indicates that we have debug modules loaded
#endif
		}
	}
#endif

	return reinterpret_cast<CSysModule *>(hDLL);
}


//-----------------------------------------------------------------------------
// Purpose: Unloads a DLL/component from
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
void Sys_UnloadModule( CSysModule *pModule )
{
	if ( !pModule )
		return;

	HMODULE	hDLL = reinterpret_cast<HMODULE>(pModule);

#ifdef _WIN32
	FreeLibrary( hDLL );
#elif defined(POSIX)
	dlclose((void *)hDLL);
#endif
}


HMODULE Sys_LoadLibrary( const char *pLibraryName, Sys_Flags flags )
{
	char str[ 1024 ];
	// Note: DLL_EXT_STRING can be "_srv.so" or "_360.dll". So be careful
	//	when using the V_*Extension* routines...
	const char *pDllStringExtension = V_GetFileExtension( DLL_EXT_STRING );
        #ifdef POSIX
            const char *pModuleExtension = ".so";
        #else
            const char *pModuleExtension = pDllStringExtension ? ( pDllStringExtension - 1 ) : DLL_EXT_STRING;
        #endif
	
	Q_strncpy( str, pLibraryName, sizeof(str) );

        V_SetExtension( str, pModuleExtension, sizeof(str) );

	Q_FixSlashes( str );

#ifdef _WIN32
	ThreadedLoadLibraryFunc_t threadFunc = GetThreadedLoadLibraryFunc();
	if ( !threadFunc )
		return InternalLoadLibrary( str, flags );

	// We shouldn't be passing noload while threaded.
	Assert( !( flags & SYS_NOLOAD ) );

	ThreadedLoadLibaryContext_t context;
	context.m_pLibraryName = str;
	context.m_hLibrary = 0;

	ThreadHandle_t h = CreateSimpleThread( ThreadedLoadLibraryFunc, &context );

	unsigned int nTimeout = 0;
	while( ThreadWaitForObject( h, true, nTimeout ) == TW_TIMEOUT )
	{
		nTimeout = threadFunc();
	}

	ReleaseThreadHandle( h );
	return context.m_hLibrary;

#elif POSIX
	int dlopen_mode = RTLD_NOW;
        
	if ( flags & SYS_NOLOAD )
		dlopen_mode |= RTLD_NOLOAD;
        
        console_print(TURBO_COLOR_YELLOW, "Try load library '%s' \n", str);
	HMODULE ret = ( HMODULE )dlopen( str, dlopen_mode );
	if ( !ret && !( flags & SYS_NOLOAD ) )
	{
		const char *pError = dlerror();
		/*if ( pError && ( strstr( pError, "No such file" ) == 0 ) && ( strstr( pError, "image not found" ) == 0 ) )
		{
			//Msg( " failed to dlopen %s error=%s\n", str, pError );
                        console_print (TURBO_COLOR_RED ,  " failed to dlopen %s error=%s\n", str, pError );
		}*/
		console_print (TURBO_COLOR_RED ,  " failed to dlopen %s error=%s\n", str, pError );
	}
	
	return ret;
#endif
}

//-----------------------------------------------------------------------------
// small helper function shared by lots of modules
//-----------------------------------------------------------------------------
bool V_IsAbsolutePath( const char *pStr )
{
	bool bIsAbsolute = ( pStr[0] && pStr[1] == ':' ) || pStr[0] == '/' || pStr[0] == '\\';
	return bIsAbsolute;
}

//-----------------------------------------------------------------------------
// Purpose: Force extension...
// Input  : *path - 
//			*extension - 
//			pathStringLength - 
//-----------------------------------------------------------------------------
void V_SetExtension( char *path, const char *extension, int pathStringLength )
{
	V_StripExtension( path, path, pathStringLength );

	// We either had an extension and stripped it, or didn't have an extension
	// at all. Either way, we need to concatenate our extension now.

	// extension is not required to start with '.', so if it's not there,
	// then append that first.
	if ( extension[0] != '.' )
	{
		V_strncat( path, ".", pathStringLength, COPY_ALL_CHARACTERS );
	}

	V_strncat( path, extension, pathStringLength, COPY_ALL_CHARACTERS );
}

char *V_strncat(char *pDest, const char *pSrc, size_t destBufferSize, int max_chars_to_copy )
{
	size_t charstocopy = (size_t)0;

	Assert( (ptrdiff_t)destBufferSize >= 0 );
	AssertValidStringPtr( pDest);
	AssertValidStringPtr( pSrc );
	
	size_t len = strlen(pDest);
	size_t srclen = strlen( pSrc );
	if ( max_chars_to_copy <= COPY_ALL_CHARACTERS )
	{
		charstocopy = srclen;
	}
	else
	{
		charstocopy = (size_t)vmin( max_chars_to_copy, (int)srclen );
	}

	if ( len + charstocopy >= destBufferSize )
	{
		charstocopy = destBufferSize - len - 1;
	}

	if ( (int)charstocopy <= 0 )
	{
		return pDest;
	}

	ANALYZE_SUPPRESS( 6059 ); // warning C6059: : Incorrect length parameter in call to 'strncat'. Pass the number of remaining characters, not the buffer size of 'argument 1'
	char *pOut = strncat( pDest, pSrc, charstocopy );
	return pOut;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the file extension within a file name string
// Input:	in - file name 
// Output:	pointer to beginning of extension (after the "."), or NULL
//				if there is no extension
//-----------------------------------------------------------------------------
const char * V_GetFileExtension( const char * path )
{
	const char    *src;

	src = path + strlen(path) - 1;

//
// back up until a . or the start
//
	while (src != path && *(src-1) != '.' )
		src--;

	// check to see if the '.' is part of a pathname
	if ( (src == path) ||  ( PATHSEPARATOR( *src ) ) )
	{		
		return NULL;  // no extension
	}

	return src;
}

int V_snprintf( char *pDest, int maxLen, char const *pFormat, ... )
{
	Assert( maxLen > 0 );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pFormat );

	va_list marker;

	va_start( marker, pFormat );
#ifdef _WIN32
	int len = _vsnprintf( pDest, maxLen, pFormat, marker );
#elif POSIX
	int len = vsnprintf( pDest, maxLen, pFormat, marker );
#else
	#error "define vsnprintf type."
#endif
	va_end( marker );

	// Len > maxLen represents an overflow on POSIX, < 0 is an overflow on windows
	if( len < 0 || len >= maxLen )
	{
		len = maxLen;
		pDest[maxLen-1] = 0;
	}

	return len;
}


void V_StripExtension( const char *in, char *out, int outSize )
{
	// Find the last dot. If it's followed by a dot or a slash, then it's part of a 
	// directory specifier like ../../somedir/./blah.

	// scan backward for '.'
	int end = V_strlen( in ) - 1;
	while ( end > 0 && in[end] != '.' && !PATHSEPARATOR( in[end] ) )
	{
		--end;
	}

	if (end > 0 && !PATHSEPARATOR( in[end] ) && end < outSize)
	{
		int nChars = vmin( end, outSize-1 );
		if ( out != in )
		{
			memcpy( out, in, nChars );
		}
		out[nChars] = 0;
	}
	else
	{
		// nothing found
		if ( out != in )
		{
			V_strncpy( out, in, outSize );
		}
	}
}

int	_V_strlen(const char* file, int line, const char *str)
{
	AssertValidStringPtr(str);
	return strlen( str );
}

void V_strncpy( char *pDest, char const *pSrc, int maxLen )
{
	Assert( maxLen >= sizeof( *pDest ) );
	AssertValidWritePtr( pDest, maxLen );
	AssertValidStringPtr( pSrc );

	strncpy( pDest, pSrc, maxLen );
	if ( maxLen > 0 )
	{
		pDest[maxLen-1] = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Changes all '/' or '\' characters into separator
// Input  : *pname - 
//			separator - 
//-----------------------------------------------------------------------------
void V_FixSlashes( char *pname, char separator /* = CORRECT_PATH_SEPARATOR */ )
{
	while ( *pname )
	{
		if ( *pname == INCORRECT_PATH_SEPARATOR || *pname == CORRECT_PATH_SEPARATOR )
		{
			*pname = separator;
		}
		pname++;
	}
}

void *Sys_GetProcAddress( HMODULE hModule, const char *pName )
{
	return (void *)dlsym( (void *)hModule, pName );
}

