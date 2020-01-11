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


/**
 * 
 * 
 *          S O U R C E   S D K    
 *              M I N  I M A L
 * 
 * 
 */

//-----------------------------------------------------------------------------
// Default console command autocompletion function 
//-----------------------------------------------------------------------------
int DefaultCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a console command
//-----------------------------------------------------------------------------
//ConCommand::ConCommand()
//{
//	m_bIsNewConCommand = true;
//}



ConCommand::ConCommand( const char *pName, FnCommandCallbackVoid_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	m_fnCommandCallbackV1 = callback;
	m_bUsingNewCommandCallback = false;
	m_bUsingCommandCallbackInterface = false;
	m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
	m_bHasCompletionCallback = completionFunc != 0 ? true : false;

	// Setup the rest
	BaseClass::CreateBase( pName, pHelpString, flags );
}

ConCommand::ConCommand( const char *pName, FnCommandCallback_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	m_fnCommandCallback = callback;
	m_bUsingNewCommandCallback = true;
	m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
	m_bHasCompletionCallback = completionFunc != 0 ? true : false;
	m_bUsingCommandCallbackInterface = false;

	// Setup the rest
	BaseClass::CreateBase( pName, pHelpString, flags );
}

ConCommand::ConCommand( const char *pName, ICommandCallback *pCallback, const char *pHelpString /*= 0*/, int flags /*= 0*/, ICommandCompletionCallback *pCompletionCallback /*= 0*/ )
{
	// Set the callback
	m_pCommandCallback = pCallback;
	m_bUsingNewCommandCallback = false;
	m_pCommandCompletionCallback = pCompletionCallback;
	m_bHasCompletionCallback = ( pCompletionCallback != 0 );
	m_bUsingCommandCallbackInterface = true;

	// Setup the rest
	BaseClass::CreateBase( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ConCommand::~ConCommand( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Invoke the function if there is one
//-----------------------------------------------------------------------------
void ConCommand::Dispatch( const CCommand &command )
{
	if ( m_bUsingNewCommandCallback )
	{
		if ( m_fnCommandCallback )
		{
			( *m_fnCommandCallback )( command );
			return;
		}
	}
	else if ( m_bUsingCommandCallbackInterface )
	{
		if ( m_pCommandCallback )
		{
			m_pCommandCallback->CommandCallback( command );
			return;
		}
	}
	else
	{
		if ( m_fnCommandCallbackV1 )
		{
			( *m_fnCommandCallbackV1 )();
			return;
		}
	}

	// Command without callback!!!
	AssertMsg( 0, "Encountered ConCommand '%s' without a callback!\n", GetName() );
}


/**
 * 
 *  B A S E
 * 
 */

//-----------------------------------------------------------------------------
// Statically constructed list of ConCommandBases, 
// used for registering them with the ICVar interface
//-----------------------------------------------------------------------------
ConCommandBase			*ConCommandBase::s_pConCommandBases = NULL;
IConCommandBaseAccessor	*ConCommandBase::s_pAccessor = NULL;
static int s_nCVarFlag = 0;
static int s_nDLLIdentifier = -1;	// A unique identifier indicating which DLL this convar came from
static bool s_bRegistered = false;


//-----------------------------------------------------------------------------
// Purpose: Default constructor
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( void )
{
	m_bRegistered   = false;
	m_pszName       = NULL;
	m_pszHelpString = NULL;

	m_nFlags = 0;
	m_pNext  = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The base console invoked command/cvar interface
// Input  : *pName - name of variable/command
//			*pHelpString - help text
//			flags - flags
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( const char *pName, const char *pHelpString /*=0*/, int flags /*= 0*/ )
{
	CreateBase( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConCommandBase::~ConCommandBase( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//			callback - 
//			*pHelpString - 
//			flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::CreateBase( const char *pName, const char *pHelpString /*= 0*/, int flags /*= 0*/ )
{
	m_bRegistered = false;

	// Name should be static data
	Assert( pName );
	m_pszName = pName;
	m_pszHelpString = pHelpString ? pHelpString : "";

	m_nFlags = flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
	m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif

	if ( !( m_nFlags & FCVAR_UNREGISTERED ) )
	{
		m_pNext = s_pConCommandBases;
		s_pConCommandBases = this;
	}
	else
	{
		// It's unregistered
		m_pNext = NULL;
	}

	// If s_pAccessor is already set (this ConVar is not a global variable),
	//  register it.
	if ( s_pAccessor )
	{
		Init();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used internally by OneTimeInit to initialize.
//-----------------------------------------------------------------------------
void ConCommandBase::Init()
{
	if ( s_pAccessor )
	{
		s_pAccessor->RegisterConCommandBase( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsCommand( void ) const
{ 
//	Assert( 0 ); This can't assert. . causes a recursive assert in Sys_Printf, etc.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return name of the command/var
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetName( void ) const
{
	return m_pszName;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flag - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsFlagSet( int flag ) const
{
	return ( flag & m_nFlags ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::AddFlags( int flags )
{
	m_nFlags |= flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
	m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const ConCommandBase
//-----------------------------------------------------------------------------
const ConCommandBase *ConCommandBase::GetNext( void ) const
{
	return m_pNext;
}

ConCommandBase *ConCommandBase::GetNext( void )
{
	return m_pNext;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetHelpText( void ) const
{
	return m_pszHelpString;
}

//-----------------------------------------------------------------------------
// Purpose: Has this cvar been registered
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsRegistered( void ) const
{
	return m_bRegistered;
}

//-----------------------------------------------------------------------------
// Returns the DLL identifier
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t ConCommandBase::GetDLLIdentifier() const
{
	return s_nDLLIdentifier;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this is a command 
//-----------------------------------------------------------------------------
bool ConCommand::IsCommand( void ) const
{ 
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calls the autocompletion method to get autocompletion suggestions
//-----------------------------------------------------------------------------
int	ConCommand::AutoCompleteSuggest( const char *partial, CUtlVector< CUtlString > &commands )
{
	if ( m_bUsingCommandCallbackInterface )
	{
		if ( !m_pCommandCompletionCallback )
			return 0;
		return m_pCommandCompletionCallback->CommandCompletionCallback( partial, commands );
	}

	Assert( m_fnCompletionCallback );
	if ( !m_fnCompletionCallback )
		return 0;

	char rgpchCommands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
	int iret = ( m_fnCompletionCallback )( partial, rgpchCommands );
	for ( int i = 0 ; i < iret; ++i )
	{
		CUtlString str = rgpchCommands[ i ];
		commands.AddToTail( str );
	}
	return iret;
}


//-----------------------------------------------------------------------------
// Returns true if the console command can autocomplete 
//-----------------------------------------------------------------------------
bool ConCommand::CanAutoComplete( void )
{
	return m_bHasCompletionCallback;
}



//-----------------------------------------------------------------------------
// Simple string class. 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Either allocates or reallocates memory to the length
//
// Allocated space for length characters.  It automatically adds space for the 
// nul and the cached length at the start of the memory block.  Will adjust
// m_pString and explicitly set the nul at the end before returning.
void *CUtlString::AllocMemory( uint32 length )
{
	void *pMemoryBlock;
	if ( m_pString )
	{
		pMemoryBlock = realloc( m_pString, length + 1 );
	}
	else
	{
		pMemoryBlock = malloc( length + 1 );
	}
	m_pString = (char*)pMemoryBlock;
	m_pString[ length ] = 0;

	return pMemoryBlock;
}

/**
 * 
 * CUtlString
 * 
 */

//-----------------------------------------------------------------------------
void CUtlString::SetDirect( const char *pValue, int nChars )
{
	if ( pValue && nChars > 0 )
	{
		if ( pValue == m_pString )
		{
			AssertMsg( nChars == Q_strlen(m_pString), "CUtlString::SetDirect does not support resizing strings in place." );
			return; // Do nothing. Realloc in AllocMemory might move pValue's location resulting in a bad memcpy.
		}

		Assert( nChars <= Min<int>( strnlen(pValue, nChars) + 1, nChars ) );
		AllocMemory( nChars );
		Q_memcpy( m_pString, pValue, nChars );
	}
	else
	{
		Purge();
	}

}


void CUtlString::Set( const char *pValue )
{
	int length = pValue ? V_strlen( pValue ) : 0;
	SetDirect( pValue, length );
}

const char *CUtlString::Get( ) const
{
	if (!m_pString)
	{
		return "";
	}
	return m_pString;
}


void CUtlString::Purge()
{
    free( m_pString );
    m_pString = NULL;
}


