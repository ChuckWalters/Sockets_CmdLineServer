#pragma once
#include "cSmartPtr.h"
#include "Winsock2.h"
#include "PacketDefs.h"
#include "cListener.h"
#include <queue>
#include "cLog.h"

class cSession
{
public:
	static volatile int gSessionNum;	

	cSession(cConnectionData * conn=NULL)
	{
		mSessionNum = ++gSessionNum;
		prn("Starting new session number: "<<mSessionNum);
		InitializeCriticalSection(&mMutex);
		mEvent = CreateEvent(NULL,true,false,"ReadyToSend");
		mSock = INVALID_SOCKET;
		mSendThreadActive = false;
		mRecvThreadActive = false;
		mClosing		  = false;

		if (conn)
		{
			mSock = conn->s;
			mAddr = conn->addr;
		}
	}
	~cSession()
	{
		prn("Closing session # "<<mSessionNum);
		close();

		EnterCriticalSection(&mMutex);
		while (!mSendQ.empty())
		{
			cPkt * pkt = (cPkt*)mSendQ.front();
			if (pkt)
			{
				mSendQ.pop();
				delete[] ((char*)pkt);
			}
		}
		LeaveCriticalSection(&mMutex);

		DeleteCriticalSection(&mMutex);
		CloseHandle(mEvent);
		prn("Session # "<<mSessionNum<<" deleted");
	}
	void
		run();
	int
		openTCP(char * IP, u_short port);
	void
		openUDP_Recv(u_short port);
	bool
		sendUDP(char * IP, u_short port, char * msg)
	{
		prn("====Sending UDP msg: "<<msg);
		SOCKET sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

		SOCKADDR_IN	addr;
		addr.sin_family		= AF_INET;
		addr.sin_port			= htons(port);
		addr.sin_addr.s_addr	= inet_addr(IP);

		cPkt_Message pkt;
		pkt.ID = kPKT_MESSAGE;
		strcpy(&pkt.instructions[1],msg);
		pkt.instructions[0] = (char)strlen(msg);

		int len = sizeof(cPkt_ID)+ (unsigned char)pkt.instructions[0]+1;
		cPkt * header = (cPkt*)new char[len + sizeof(cPkt)];
		header->mLen = (short)(len + sizeof(cPkt));
		memcpy(&header[1],&pkt,len);
		int err = sendto(sock,(char*)header,header->mLen,0,(SOCKADDR*)&addr,sizeof(addr));
		if (err == header->mLen)
		{
			prn("====Sent "<<err<<" bytes over udp");
			return true;
		}
		else
		{
			prn("====Err: udp send returned "<<err<<" wsaerr# "<<WSAGetLastError());
			return false;
		}
	}
	cPkt_ID *
		recvUDPNow(u_short port)
	{
		if (mSock == INVALID_SOCKET)
			mSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

		mAddr.sin_family		= AF_INET;
		mAddr.sin_port			= htons(port);
		mAddr.sin_addr.s_addr	= htonl(INADDR_ANY);

		bind(mSock, (SOCKADDR*)&mAddr, sizeof(mAddr));
		char	buf[512];
		int		addrLen = 0;
		int err = recvfrom(mSock,buf,sizeof(buf),0, (SOCKADDR*)&mAddr, &addrLen);
		if (err <= 0)
		{
			prn ("recvfrom err: "<<err);
			return NULL;
		}
		cPkt * header = (cPkt *)buf;
		// verify length
		if (header->mLen > 512)
		{
			prn ("recvfrom err: Invalid pkt len = "<<header->mLen);
			return NULL;
		}
		return convertToProperPktFormat((char*)&header[1], header->mLen - sizeof(cPkt));
	}
	void
		close()
	{
		struct linger lingerTime;
		lingerTime.l_onoff = TRUE;
		lingerTime.l_linger = 3; // Time in seconds
		setsockopt( mSock, SOL_SOCKET, SO_LINGER, (char*)&lingerTime, sizeof(lingerTime));

		shutdown( mSock, SD_BOTH );
		mClosing = true;

		closesocket(mSock);
		mSock = INVALID_SOCKET;

		if (mEvent)
			SetEvent(mEvent);	
		while (mSendThreadActive==true || mRecvThreadActive==true)
		{
			Sleep(10);
		}
	}
	void
		waitForRemoteClosure(int giveUpTime, int secondsTimeOut=2)
	{
		for (int cnt=0; mRecvThreadActive==true && cnt<giveUpTime; cnt+=secondsTimeOut)
		{
			Sleep(secondsTimeOut*1000);
		}
	}
	void
		waitForEmptySendQ()
	{
		while (mSock != INVALID_SOCKET && !mSendQ.empty())
		{
			Sleep(5);
		}
	}
	void
		sendNetID(char * msg)
	{
		cPkt_StudentInfo pkt;
		pkt.ID = kPKT_STUDENTINFO;
		strcpy(pkt.name,msg);
		sendPkt(&pkt,sizeof(pkt));
	}
	void
		sendAddress(char * ip, unsigned short port)
	{
		prn("====Sending TCP addr: "<<ip<<":"<<port);

		cPkt_Address pkt;
		pkt.ID		= kPKT_ADDRESS;
		pkt.port	= port;
		memset(pkt.address, 0, sizeof(pkt.address) );
		strcpy(pkt.address,ip);
		sendPkt(&pkt,sizeof(pkt));
	}
	void
		sendMsg(const char * msg)
	{
		prn("====Sending TCP msg ("<<(u_int)strlen(msg)<<"): "<<msg);
		
		if(strlen(msg)>256)
		{
			throw "Message too long";
			return; // error
		}

		cPkt_Message pkt;
		pkt.ID = kPKT_MESSAGE;
		strcpy(&pkt.instructions[1],msg);
		pkt.instructions[0] = (char)strlen(msg);
		sendPkt(&pkt,(u_int)sizeof(cPkt_ID)+ strlen(msg)+1);		
	}
	void
		sendPkt(void * data, int len)
	{
		cPkt * pkt = (cPkt*)new char[len + sizeof(cPkt)];
		pkt->mLen = (short)(len + sizeof(cPkt));
		memcpy(&pkt[1],data,len);
		EnterCriticalSection(&mMutex);
		mSendQ.push(pkt);
		LeaveCriticalSection(&mMutex);
		SetEvent(mEvent);	
	}
	cPkt_ID *
		recvPkt(DWORD msTimeOut=0)
	{
		if (!msTimeOut)
		{
			if (mRecvQ.empty())
			{
				return NULL;
			}
		}
		else
		{
			DWORD startTick = GetTickCount();

			while (mRecvQ.empty())
			{
				if ( (startTick+msTimeOut) < GetTickCount() )
				{
					sendMsg("Server timed out waiting for a packet from you");
					prn("Manually timed out waiting for receive");
					return NULL;
				}
				if (mSock == INVALID_SOCKET || !mRecvThreadActive)
				{
					prn("Socket died during recv");
					return NULL;
				}
				Sleep(250);
			}
		}
		EnterCriticalSection(&mMutex);
		cPkt_ID * pkt;
		pkt = mRecvQ.front();
		mRecvQ.pop();
		LeaveCriticalSection(&mMutex);
		return pkt;
	}

	void
		udpSendThread();
	void
		udpRecvThread();
	void
		tcpSendThread();
	void
		tcpRecvThread();
private:
	static void __cdecl
		udpSendThreadWrapper(void * data)
	{
		static_cast<cSession *>(data)->udpSendThread(); 
	}
	static void __cdecl
		udpRecvThreadWrapper(void * data)
	{
		static_cast<cSession *>(data)->udpRecvThread(); 
	}
	static void __cdecl
		tcpSendThreadWrapper(void * data)
	{
		static_cast<cSession *>(data)->tcpSendThread(); 
	}
	static void __cdecl
		tcpRecvThreadWrapper(void * data)
	{
		static_cast<cSession *>(data)->tcpRecvThread(); 
	}
	BOOL
		DoRecv(char * buf, int len);

	cPkt_ID *
		convertToProperPktFormat(char * data, int len)
	{
		cPkt_ID * pkt;
		char tStr[256];
		int desiredLen;

		// Set up proper class
		// -------------------
		switch(*data)
		{
		case kPKT_STUDENTINFO:
			desiredLen = sizeof(cPkt_StudentInfo);
			if( len != desiredLen )
			{
				sprintf(tStr, "Invalid kPKT_STUDENTINFO, expecting total packet size of: %d, received: %d", desiredLen+sizeof(cPkt), len+sizeof(cPkt));
				sendMsg(tStr);
				return NULL;
			}
			pkt = new cPkt_StudentInfo();
			break;
		case kPKT_MESSAGE:
			{
				pkt = new cPkt_Message();
				int maxstrlen = sizeof(((cPkt_Message*)pkt)->instructions);
				memset(((cPkt_Message*)pkt)->instructions,0,maxstrlen);
			}
			break;
		case kPKT_ADDRESS:
			desiredLen = sizeof(cPkt_Address);
			if( len != desiredLen )
			{
				sprintf(tStr, "Invalid kPKT_ADDRESS, expecting total packet size of: %d, received: %d", desiredLen+sizeof(cPkt), len+sizeof(cPkt));
				sendMsg(tStr);
				return NULL;
			}
			pkt = new cPkt_Address();
			break;
		default:
			sprintf(tStr,"Error: Received invalid packet ID: %d",*data);
			sendMsg(tStr);
			return NULL;
		}
		memcpy(pkt,data,len);
		return pkt;
	}
	bool
		pushRecvData(char * data, int len)
	{
		cPkt_ID * pkt = convertToProperPktFormat(data, len);

		EnterCriticalSection(&mMutex);
		mRecvQ.push(pkt);
		LeaveCriticalSection(&mMutex);
		return true;
	}

	int 
		mSessionNum;	
	volatile bool
		mClosing;	
	volatile SOCKET 
		mSock;
	SOCKADDR_IN	
		mAddr;
	std::queue<cPkt *> 
		mSendQ;
	std::queue<cPkt_ID *> 
		mRecvQ;
	CRITICAL_SECTION  
		mMutex;			// Crit Section for safe q access (should have one per Q to avoid unnecessary blocks)
	HANDLE			
		mEvent;
	HANDLE
		mCompletionPort;
public:
	volatile bool 
		mSendThreadActive;
	volatile bool 
		mRecvThreadActive;
};

