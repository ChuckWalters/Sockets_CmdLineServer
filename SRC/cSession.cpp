#include "stdafx.h"
#include "cSession.h"

volatile int 
cSession::gSessionNum = 0;

void
cSession::run()
{
	mSendThreadActive = true;
	mRecvThreadActive = true;

   _beginthread (tcpSendThreadWrapper, 0, this);
   _beginthread (tcpRecvThreadWrapper, 0, this);
}
int
cSession::openTCP(char * IP, u_short port)
{
	mSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	mAddr.sin_family		= AF_INET;
	mAddr.sin_port			= htons(port);
	mAddr.sin_addr.s_addr	= inet_addr(IP);

	int nameLen = sizeof(mAddr);

#if 0
	// IP6 compatible address setup
	PADDRINFOT	pAddrInfo = NULL;
	ADDRINFOT	hints;
	memset(&hints,0,sizeof(hints));
	hints.ai_flags		= AI_NUMERICHOST;
	hints.ai_family		= PF_INET;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_protocol	= IPPROTO_TCP;

	char strPort[10];
	itoa(port,strPort,10);

	int err = getaddrinfo( IP, strPort, &hints, &pAddrInfo );
#endif

	int rv = connect(mSock,(SOCKADDR*)&mAddr,nameLen);
	if (rv == 0)
	{
		prn("====Connected to socket " << IP <<"::"<<port);
		return 0;
	}
	else
	{
		int error = WSAGetLastError();
		prn("Failed connecting to socket " << IP <<"::"<<port<<"  err# "<<error);
		return error;
	}
}
void
cSession::openUDP_Recv(u_short port)
{
	mSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	mAddr.sin_family		= AF_INET;
	mAddr.sin_port			= htons(port);
	mAddr.sin_addr.s_addr	= htonl(INADDR_ANY);

	bind(mSock,(SOCKADDR*)&mAddr,sizeof(mAddr));

	mRecvThreadActive = true;

   _beginthread (udpRecvThreadWrapper, 0, this);
}
void
cSession::udpSendThread()
{
}
void
cSession::udpRecvThread()
{
	char	buf[512];
	int		addrLen = sizeof(mAddr);
	int err;
	do
	{
		if (mSock == INVALID_SOCKET)
			break;

		err = recvfrom(mSock,buf,sizeof(buf),0, (SOCKADDR*)&mAddr, &addrLen);
		if (mClosing)
			break;
		if (err <= 0)
		{
			prn ("# "<<mSessionNum<<" recvfrom err: "<<err<<" wsaerr# "<<WSAGetLastError());
			break;
		}
		prn ("# "<<mSessionNum<<" recvfrom got "<<err<<" bytes");

		// Add it to the read queue
		// ------------------------
		pushRecvData(&buf[sizeof(cPkt)],err-sizeof(cPkt));
	}while(err>0);

	closesocket(mSock);
	mSock = INVALID_SOCKET;
	mRecvThreadActive = false;
}
void
cSession::tcpSendThread()
{
	int		err;
	short	len;

	while (WAIT_OBJECT_0 == WaitForSingleObject(mEvent,INFINITE))
	{
		if (mClosing)
			break;

		len = 0;
		while (!mSendQ.empty())
		{
			EnterCriticalSection(&mMutex);
			cPkt * pkt = (cPkt*)mSendQ.front();
			mSendQ.pop();
			LeaveCriticalSection(&mMutex);

			len = pkt->mLen;
			err = send(mSock,(char*)pkt,len,0);

			delete[] ((char*)pkt);

			if (err < len)
			{
				len = -1;
				prn("# "<<mSessionNum<<" Error on send "<<err);
				break;
			}
			else
			{
				char tmpAddrStr[60];
				memset(tmpAddrStr,0,60);
				strncpy(tmpAddrStr,inet_ntoa(mAddr.sin_addr),59);

				prn("# "<<mSessionNum<<" TCP sent "<<len<<" bytes to "<<tmpAddrStr<<":"<<mAddr.sin_port);
			}
		}
		if (mSock == INVALID_SOCKET)
			break;
		if (len == -1)
			break;
	}
	prn("Closing Send thread for session # "<<mSessionNum);
	mSendThreadActive = false;
}
BOOL
cSession::DoRecv(char * buf, int len)
{
	int	err=1;
	int offset=0;
	while( err > 0 )
	{
		err = recv(mSock,buf+offset,len,0);
		if (mClosing)
		{
			return FALSE;
		}
		if( err == len )
		{
			return TRUE;
		}
		else if( err < 0 )
		{	
			prn("# "<<mSessionNum<<" Error: header recv: "<<err<< "  err# "<<WSAGetLastError());
			return FALSE;
		}
		else if( 0 == err )
		{	
			prn("# "<<mSessionNum<<" Connection closed: recv returned 0");
			return FALSE;
		}

		prn("# "<<mSessionNum<<" Partial recv of "<<err<<" bytes");

		offset = err;
		len -= err;
	};
	throw "Error: DoRecv() should not get here";
	return FALSE;
}
void
cSession::tcpRecvThread()
{
	cPkt	header;
	char	buff[512];
	do
	{	
		// Always read header first
		// ------------------------
		int len = sizeof(cPkt);
		int offset=0;
		prn("# "<<mSessionNum<<" tcp recv header");
		if( !DoRecv( (char*)&header, len ) )
			break;

		// Validate Header
		// Send an error message back to student
		// ---------------
		if ( header.mLen < (sizeof(cPkt)+sizeof(cPkt_ID)) || header.mLen > 512)
		{
			char msg[80];
			sprintf(msg,"Error: Bad packet length value: %d",header.mLen);
			sendMsg(msg);
			break;
		}
		// Now read data
		// -------------
		len = header.mLen-sizeof(cPkt);
		prn("# "<<mSessionNum<<" tcp recv data, len: "<<len);
		if( !DoRecv( buff, len ) )
			break;

		// Add it to the read queue
		// ------------------------
		pushRecvData(buff,len);

	}while( !mClosing );

	prn("# "<<mSessionNum<<" Closing Recv thread");
	mRecvThreadActive = false;
}
