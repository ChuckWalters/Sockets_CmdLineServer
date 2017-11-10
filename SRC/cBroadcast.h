#pragma once
#include "stdafx.h"

class cBroadcast
{
public:
	/// =======================================================================
	///
	/// =======================================================================
	cBroadcast()
	{
		m_SendThreadData.quitEvent = NULL;
		m_RecvThreadData.quitEvent = NULL;
		m_SendThreadData.data	= NULL;
	}
	/// =======================================================================
	///
	/// =======================================================================
	~cBroadcast()
	{
		if (m_SendThreadData.quitEvent)
			CloseHandle(m_SendThreadData.quitEvent);
		if (m_RecvThreadData.quitEvent)
			CloseHandle(m_RecvThreadData.quitEvent);
	}
	/// =======================================================================
	///
	/// =======================================================================
	bool
	cBroadcast::startSending(char *IP, u_short port, char * data, int dataLen, DWORD msTimeOut)
	{
	// Check if already broadcasting
	// -----------------------------
	if (m_SendThreadData.quitEvent)
		return false;

	// Make sure there's data
	// ----------------------
	if (!dataLen)
		return false;

	// Create the socket handle
	// If it fails, then the system is out of handles
	// Go ahead and stick this in a loop and count how 
	// many handles you can create before it errors.
	// ----------------------------------------------
	m_SendThreadData.sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (INVALID_SOCKET == m_SendThreadData.sock)
		return false;

	// Setup the data for the thread to send the broadcast
	// ---------------------------------------------------
	m_SendThreadData.addr.sin_family		= AF_INET;
	m_SendThreadData.addr.sin_addr.s_addr	= inet_addr(IP);
	m_SendThreadData.addr.sin_port			= htons(port);
	m_SendThreadData.msTimeOut				= msTimeOut;
	m_SendThreadData.quitEvent				= CreateEvent(NULL,true,false,"QuitBroadcastSend");
	m_SendThreadData.dataLen				= dataLen;
	m_SendThreadData.data					= new char[dataLen];
	memcpy(m_SendThreadData.data,data,dataLen);

	// Start the thread passing this object in as data,
	// see the global thread function Thread_SendBroadcast() where
	// the this pointer is used to redirect to a public method of 
	// this class...
	// -----------------------------------------------------------
   _beginthread (cBroadcast::Thread_SendBroadcast, 0, this);

	return true;
	}
	/// =======================================================================
	///
	/// =======================================================================
	bool
	cBroadcast::startRecving(u_short port, DWORD msTimeOut)
	{
		if (m_RecvThreadData.quitEvent)
			return false;
	
		m_RecvThreadData.sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (INVALID_SOCKET == m_RecvThreadData.sock)
			return false;

		m_RecvThreadData.addr.sin_family		= AF_INET;
		m_RecvThreadData.addr.sin_addr.s_addr	= INADDR_BROADCAST;
		m_RecvThreadData.addr.sin_port			= htons(port);
		m_RecvThreadData.msTimeOut				= msTimeOut;
		m_RecvThreadData.quitEvent				= CreateEvent(NULL,true,false,"QuitBroadcastRecv");
		m_RecvThreadData.dataLen				= 1024;
		if (m_RecvThreadData.dataLen)
			m_RecvThreadData.data				= new char[m_RecvThreadData.dataLen];

		_beginthread(cBroadcast::Thread_RecvBroadcast, 0, this);

		return true;
	}
	/// =======================================================================
	///
	/// =======================================================================
	void
	stopSending()
	{
		SetEvent(m_SendThreadData.quitEvent);
	}
	/// =======================================================================
	///
	/// =======================================================================
	void
	stopRecving()
	{
		SetEvent(m_RecvThreadData.quitEvent);
	}
	/// =======================================================================
	///
	/// =======================================================================
	void
	cBroadcast::SendThread(void)
	{
		int err;
		BOOL enable = true;
		err = setsockopt( m_SendThreadData.sock, SOL_SOCKET, SO_BROADCAST, (const char*)&enable, sizeof(enable));

		do
		{
			err = sendto(	m_SendThreadData.sock, 
							m_SendThreadData.data,	
							m_SendThreadData.dataLen,
							0,
							(LPSOCKADDR)&m_SendThreadData.addr,
							sizeof(SOCKADDR));

		}while (WAIT_TIMEOUT == WaitForSingleObject(m_SendThreadData.quitEvent,m_SendThreadData.msTimeOut));
	}
	/// =======================================================================
	///
	/// =======================================================================
	void
	cBroadcast::RecvThread(void)
	{
		int err;

		do
		{
			int dataLen = sizeof(SOCKADDR);
			err = recvfrom(	m_RecvThreadData.sock, 
							m_RecvThreadData.data,	
							m_RecvThreadData.dataLen,
							0,
							(LPSOCKADDR)&m_RecvThreadData.addr,
							&dataLen);

		}while (WAIT_TIMEOUT == WaitForSingleObject(m_RecvThreadData.quitEvent,m_RecvThreadData.msTimeOut));
	}
private:

	/// =======================================================================
	///
	/// =======================================================================
	static void __cdecl
	Thread_SendBroadcast(void * data)
	{
		static_cast<cBroadcast *>(data)->SendThread(); 
	}
	/// =======================================================================
	///
	/// =======================================================================
	static void __cdecl
	Thread_RecvBroadcast(void * data)
	{
		static_cast<cBroadcast *>(data)->RecvThread(); 
	}

	typedef struct
	{
//		GUID;
		unsigned short	port;
		char			gameName[80];
	}PKT_Broadcast;
	typedef struct
	{
		SOCKADDR_IN		fromAddr;
		unsigned short	port;
		char			gameName[80];
	}Incomming_Broadcast_Data;
	typedef struct
	{
		SOCKADDR_IN		addr;
		char			*data;
		int				dataLen;
		DWORD			msTimeOut;
		HANDLE			quitEvent;
		SOCKET			sock;
	}THREADDATA_Broadcast;

	THREADDATA_Broadcast m_SendThreadData;
	THREADDATA_Broadcast m_RecvThreadData;
};
