#pragma once
#include "Winsock2.h"

class cWinsockInit
{
public:
	cWinsockInit(WORD majorVersion=1, WORD minorVersion=1, BOOL acceptOtherVersions=false )
	{
		int err = WSAStartup( MAKEWORD( majorVersion, minorVersion ), &wsaData );

		switch ( err ) 
		{
		case 0:
			break;
		case WSASYSNOTREADY:
			throw "WSA SYS NOT READY";
		case WSAVERNOTSUPPORTED:
			throw "WSA VER NOT SUPPORTED";
		case WSAEINPROGRESS:
			throw "WSA E IN PROGRESS";
		case WSAEPROCLIM:
			throw "WSA E PROC LIM";
		default:
			throw "WSAStartup Error";
		}
 
		if (!acceptOtherVersions)
		{
			// Confirm that the WinSock DLL supports requested version.
			// Note that if the DLL supports versions greater    
			// than requested ver. in addition to requested ver., 
			// it will still return requested version.
			// WSAStartup() may succeed without error even if requeted
			// ver. is not available, so the ver. must be checked if 
			// depending on features from the given ver.
			// -------------------------------------------------------
			if ( LOBYTE( wsaData.wVersion ) != majorVersion ||
				HIBYTE( wsaData.wVersion ) != minorVersion ) 
			{
				// ----------------------------------------------------------
				// Tell the user that we could not find a usable WinSock DLL.
				// ----------------------------------------------------------
				WSACleanup( );
				throw "WSAStartup unsupported version"; 
			}
		}
	}
	~cWinsockInit()
	{
		WSACleanup( );
	}

public:
	WSADATA wsaData;

private:
};

