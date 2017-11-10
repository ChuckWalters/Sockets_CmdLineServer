#pragma once
#include "stdafx.h"
#include "cLog.h"


class cConnectionData
{
public:
	cConnectionData()
	{
		addrLen = sizeof(addr);
	}
	SOCKET		s;
	SOCKADDR_IN	addr;
	int			addrLen;
};

class cListener
{
public:
	cListener()
	{
		InitializeCriticalSection(&mMutex);
		listeningSocket = INVALID_SOCKET;
	}
	~cListener()
	{
		stopListening();
		DeleteCriticalSection(&mMutex);
	}

public:
	bool
		isEmpty()
	{
		return q.empty();
	}
	 
	bool		
		pop(cConnectionData& data)
	{
		if (isEmpty())
			return false;
		EnterCriticalSection(&mMutex);
		data = q.front();
		q.pop();
		LeaveCriticalSection(&mMutex);
		return true;
	}
	///
	///
	///
	void
		startListening(u_short port, char *IP=NULL)
	{
		listeningSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

		prn("Listening on port # "<<port<<endl);

		SOCKADDR_IN		listenOnAddr;
		listenOnAddr.sin_family	= AF_INET;
		listenOnAddr.sin_port	= htons(port);
		if (IP)
			listenOnAddr.sin_addr.s_addr	= inet_addr(IP);
		else
			listenOnAddr.sin_addr.s_addr	= htonl(INADDR_ANY);

		bind(listeningSocket,(SOCKADDR*)&listenOnAddr,sizeof(listenOnAddr));

		_beginthread (ListenThreadWrapper, 0, this);

	}
	void
		stopListening()
	{
		if (listeningSocket != INVALID_SOCKET)
		{
			closesocket(listeningSocket);
			listeningSocket = INVALID_SOCKET;
		}
	}
	///
	void
		listenThread()
	{
		int err = listen(listeningSocket,5);
		if (err != 0)
		{
			prn("Listen returned err # "<<WSAGetLastError());
			return;
		}

		while(1)
		{
			cConnectionData data;
			data.s = accept(listeningSocket,(SOCKADDR*)&data.addr,&data.addrLen);
			if (data.s == INVALID_SOCKET)
			{
				prn("Listener received invalid socket");
				return; // end the thread
			}
			char tmpAddrStr[60];
			memset(tmpAddrStr,0,60);
			const char * addr = inet_ntoa(data.addr.sin_addr);
			if( NULL == addr )
			{
				strcpy( tmpAddrStr, "unknown" );
			}
			else
			{
				strncpy(tmpAddrStr, addr,59);
			}
			prn("Listener received connection req: "<< tmpAddrStr<<":"<<ntohs(data.addr.sin_port));
			push(data);
		}
	}

private:
	void 
		push(cConnectionData& data)
	{
		EnterCriticalSection(&mMutex);
		q.push(data);
		LeaveCriticalSection(&mMutex);
	}
	static void __cdecl
	ListenThreadWrapper(void * data)
	{
		static_cast<cListener *>(data)->listenThread(); 
	}

	std::queue<cConnectionData> 
		q;				// A queue of connections
	CRITICAL_SECTION  
		mMutex;			// Crit Section for safe q access
	volatile SOCKET	
		listeningSocket;// Socket used to listen for connections
};

