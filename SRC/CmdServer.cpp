// CmdServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "cWinsockInit.h"
#include "cListener.h"
#include "cSession.h"
#include "cHTTP.h"
#include <time.h>
#include <fstream>
#include "cLog.h"

CRITICAL_SECTION  gPrintMutex; // used for print output so message aren't collaged

const char *gGradesDir = "C:\\UW Game Dev\\Grades\\Fall04\\BlackBox";
const char *StrSendAddrPkt = "Send a cPkt_Address to the TCP line you're hosting.  Specify the address you will receive UDP traffic on.  Next instructions will be delivered over that UDP connection.  Start receiving on the UDP channel before sending the address.";

const int numNetIds = 19;
char netIDs[numNetIds][32]=
{
"anon",
"cwmail"
};
const DWORD ONE_SEC = 1000;
const DWORD STANDARD_TIMEOUT = 1000 * 30;
static void __cdecl
runServerSession(void * data)
{
	cConnectionData conn = *((cConnectionData *)data);
	delete ((cConnectionData *)data);

	// 1st TCP session
	// ----------------------------------------------------
	cSession session1(&conn);
	session1.run();

	// Wait for uwNetID
	// ----------------
	prn("====Waiting for UWNetID ");
	cPkt_ID * pkt = session1.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
		return;
	if (pkt->ID != kPKT_STUDENTINFO)
		return;

	// Recved uwNetID
	// --------------
	cPkt_StudentInfo *msgStudentInfo = (cPkt_StudentInfo*)pkt;
	CString studentID = msgStudentInfo->name;
	prn("====Recved Student ID: "<<msgStudentInfo->name);
	int index;
	for (index=0; index<numNetIds; index++)
	{
		if (!_stricmp(msgStudentInfo->name,netIDs[index]))
			break;
	}
	if (index>=numNetIds)
	{
		prn("Invalid: "<<msgStudentInfo->name);
		char outMsg[200];
		sprintf(outMsg,"Server received an invalid uwnetid: %s",msgStudentInfo->name);
		session1.sendMsg(outMsg);
		return;
	}
	
	// Open a file to log progress
	// ---------------------------
	ofstream file;
	char fname[128];
	sprintf(fname,"%s\\%s.txt",gGradesDir,netIDs[index]);
	file.open(fname,ios_base::out|ios_base::app);

	// time stamp
	// ----------
    struct tm *newtime;
    __time64_t long_time;
    _time64( &long_time );                /* Get time as long integer. */
    newtime = _localtime64( &long_time ); /* Convert to local time. */
    file <<" "<<endl<<"================================================================"<<endl;
    file << asctime( newtime )<<endl;

	// log id
	// ------
	file << "Student ID: "<<msgStudentInfo->name<<endl;

	char tmpAddrStr[60];
	memset(tmpAddrStr,0,60);
	strncpy(tmpAddrStr,inet_ntoa(conn.addr.sin_addr),59);

	file << "IP: "<<tmpAddrStr<<endl;

	// Send first instructions
	// -----------------------
	session1.sendMsg("Host a TCP connection.  Send the address you are listening on in a cPkt_Address packet."); 

	// Wait for Address packet
	// -----------------------
	prn("====Waiting for a Address pkt to connect to a new TCP socket");
	pkt = session1.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
	{
		file.close();
		return;
	}
	if (pkt->ID != kPKT_ADDRESS)
	{
		file.close();
		return;
	}
	cPkt_Address *msgAddr = (cPkt_Address*)pkt;
	file << "Recved TCP 2 Address: "<<msgAddr->address<<":"<<msgAddr->port<<endl;
	prn("====Recved TCP 2 Address: "<<msgAddr->address<<":"<<msgAddr->port<<endl);

	// Connect to Address
	// ------------------
	cSession session2;
	int error = session2.openTCP(msgAddr->address,msgAddr->port);
	if (error != 0)
	{
		session1.sendMsg("Server timed out waiting for Address packet containing UDP receiving address"); 
		file.close();
		return;
	}
	session2.run();
	file << "Connected to TCP 2: "<<msgAddr->address<<":"<<msgAddr->port<<endl;

	// Send next Instruction on new connection
	// ---------------------------------------
	session2.sendMsg(StrSendAddrPkt);

	// Wait for Address packet
	// -----------------------
	prn("====Waiting for a Address pkt to send a UDP message to");
	pkt = session2.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
	{
		session1.sendMsg("Server timed out waiting to Address packet containing UDP receiving address"); 
		file.close();
		return;
	}
	if (pkt->ID != kPKT_ADDRESS)
	{
		session1.sendMsg("Expecting kPKT_ADDRESS, received some other pkt ID"); 
		file.close();
		return;
	}
	msgAddr = (cPkt_Address*)pkt;
	prn("====Recved UDP Address: "<<msgAddr->address<<":"<<msgAddr->port);
	file << "Recved UDP Address on TCP 2: "<<msgAddr->address<<":"<<msgAddr->port<<endl;

	// Send a UDP message
	// ------------------
	cSession session3;
	session3.openUDP_Recv(20222);
	for (int sendCounter=10;sendCounter>0;--sendCounter)
	{
		session3.sendUDP(msgAddr->address,msgAddr->port,
			"Send a datagram cPkt_Message to this servers IP on port 20222.  Send the word \"ACK\" in the instructions member.  This message will be sent every 5 seconds 10 times or until your \"ACK\" message is received.");
		pkt = session3.recvPkt();
		if (pkt)
			break;
		if (!session2.mRecvThreadActive || !session1.mRecvThreadActive)
			break;
		Sleep(5000);
	}
	if (!pkt)
	{
		session1.sendMsg("Failed to get UDP ACK, Exiting"); 
		file << "Failed to get UDP ACK, Exiting"<<endl;
		file.close();
		return;
	}

	if (pkt->ID != kPKT_MESSAGE)
	{
		prn("===Invalid UDP packet ID: "<<pkt->ID);
		file << "Invalid UDP packet ID: "<<pkt->ID<<endl;
		return;
	}
	cPkt_Message *msg = (cPkt_Message *)pkt;
	prn("Recv Msg: "<<&msg->instructions[1]);
	file << "Recved UDP Msg, msg bytes="<<(unsigned int)msg->instructions[0]<<" msg: "<<&msg->instructions[1]<<endl;
	if (_strnicmp("ACK",&msg->instructions[1],3))
	{
		char invalidStr[300];
		sprintf(invalidStr,"Invalid UDP msg: %s",&msg->instructions[1]);
		session2.sendMsg(invalidStr);
		prn("==="<<invalidStr);
		file << invalidStr <<endl;
		session2.waitForEmptySendQ();
		return;
	}

	session2.sendMsg("You have successfully completed Mission IV.");
	session2.sendMsg("The instructions for covert ops are at http://courses.washington.edu/gamedev/C4/extracredit.html#section4_textarea5_heading");
	session2.waitForEmptySendQ();
	Sleep(5000);

	// Exit
	// -------
	file << "Successfully completed"<<endl;

	prn("===="<<"Exiting service with: "<<studentID.GetBuffer()<<" ====");
	studentID.ReleaseBuffer();
	session2.waitForRemoteClosure(10);
	file.close();
}

static void __cdecl
runServerSessionCompletionPorts(void * data)
{
	cConnectionData conn = *((cConnectionData *)data);
	delete ((cConnectionData *)data);

	// 1st TCP session
	// ----------------------------------------------------
	cSession session1(&conn);
	session1.run();

	// Wait for uwNetID
	// ----------------
	prn("====Waiting for UWNetID ");
	cPkt_ID * pkt = session1.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
		return;
	if (pkt->ID != kPKT_STUDENTINFO)
		return;

	// Recved uwNetID
	// --------------
	cPkt_StudentInfo *msgStudentInfo = (cPkt_StudentInfo*)pkt;
	CString studentID = msgStudentInfo->name;
	prn("====Recved Student ID: "<<msgStudentInfo->name);
	int index;
	for (index=0; index<numNetIds; index++)
	{
		if (!_stricmp(msgStudentInfo->name,netIDs[index]))
			break;
	}
	if (index>=numNetIds)
	{
		prn("Invalid: "<<msgStudentInfo->name);
		char outMsg[200];
		sprintf(outMsg,"Server received and invalid uwnetid: %s",msgStudentInfo->name);
		session1.sendMsg(outMsg);
		return;
	}
	
	// Open a file to log progress
	// ---------------------------
	ofstream file;
	char fname[128];
	sprintf(fname,"%s\\%s_CovertOps.txt",gGradesDir,netIDs[index]);
	file.open(fname,ios_base::out|ios_base::app);

	// time stamp
	// ----------
    struct tm *newtime;
    __time64_t long_time;
    _time64( &long_time );                /* Get time as long integer. */
    newtime = _localtime64( &long_time ); /* Convert to local time. */
    file <<" "<<endl<<"================================================================"<<endl;
    file << asctime( newtime )<<endl;
	// log id
	// ------
	file << "Student ID: "<<msgStudentInfo->name<<endl;

	// Send first instructions
	// -----------------------
	session1.sendMsg("Host a TCP connection.  Send the address you are listening on in a cPkt_Address packet."); 

	// Wait for Address packet
	// -----------------------
	prn("====Waiting for a Address pkt to connect to a new TCP socket");
	pkt = session1.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
	{
		file.close();
		return;
	}
	if (pkt->ID != kPKT_ADDRESS)
	{
		file.close();
		return;
	}
	cPkt_Address *msgAddrTCP = (cPkt_Address*)pkt;
	file << "Recved TCP 2 Address: "<<msgAddrTCP->address<<":"<<msgAddrTCP->port<<endl;

	// Connect to Address
	// ------------------
	cSession session2;
	int error = session2.openTCP(msgAddrTCP->address,msgAddrTCP->port);
	if (error != 0)
	{
		file.close();
		return;
	}
	session2.run();
	file << "Connected to TCP 2: "<<msgAddrTCP->address<<":"<<msgAddrTCP->port<<endl;

	// Send next Instruction on new connection
	// ---------------------------------------
	session2.sendMsg("Send a cPkt_Address to the TCP line you're hosting.  Specify the address you will receive UDP traffic on.  Next instructions will be delivered over that UDP connection.  Start receiving on the UDP channel before sending the address.");

	// Wait for Address packet
	// -----------------------
	prn("====Waiting for a Address pkt to send a UDP message to");
	pkt = session2.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
	{
		file.close();
		return;
	}
	if (pkt->ID != kPKT_ADDRESS)
	{
		file.close();
		return;
	}
	cPkt_Address *	msgAddrUDP = (cPkt_Address*)pkt;
	prn("====Recved UDP Address: "<<msgAddrUDP->address<<":"<<msgAddrUDP->port);
	file << "Recved UDP Address on TCP 2: "<<msgAddrUDP->address<<":"<<msgAddrUDP->port<<endl;

	// Send a UDP message
	// ------------------
	cSession session3;
	session3.openUDP_Recv(20333);
	for (int sendCounter=10;sendCounter>0;--sendCounter)
	{
		session3.sendUDP(msgAddrUDP->address,msgAddrUDP->port,
			"Send a datagram cPkt_Message to this servers IP on port 20333.  Send the word \"ACK\" in the instructions member.  This message will be sent every 5 seconds 10 times or until your \"ACK\" message is received.");
		pkt = session3.recvPkt();
		if (pkt)
			break;
		Sleep(5000);
	}
	if (!pkt)
	{
		file << "Failed to get UDP ACK, Exiting"<<endl;
		file.close();
		return;
	}

	if (pkt->ID != kPKT_MESSAGE)
	{
		prn("===Invalid UDP packet ID: "<<pkt->ID);
		file << "Invalid UDP packet ID: "<<pkt->ID<<endl;
		return;
	}
	cPkt_Message *msg = (cPkt_Message *)pkt;
	prn("Recv Msg: "<<&msg->instructions[1]);
	file << "Recved UDP Msg, msg bytes="<<(unsigned int)msg->instructions[0]<<" msg: "<<&msg->instructions[1]<<endl;
	if (_strnicmp("ACK",&msg->instructions[1],3))
	{
		prn("===Msg Invalid");
		file << "UDP Msg Invalid"<<endl;
		return;
	}

	session2.sendMsg("Server connecting as many times as possible to TCP socket you are hosting.  Connection attempts will stop with the first failure.  2,000 connections are required for full power-ups.");

	std::queue<cSession *> q;
	int count=0;
	for (count=0;1;count++)
	{
		cSession * session = new cSession();
		q.push(session);
		int error = session->openTCP(msgAddrTCP->address,msgAddrTCP->port);
		if (error != 0)
		{
			file << "connection err #"<<error<< endl;
			break;
		}
	}

	while(!q.empty())
	{
		cSession * s = q.front();
		q.pop();
		delete s;
	}

	char cntMsg[256];
	if (count < 2000)
	{
		sprintf(cntMsg,"Not enough connections made, only %d successful connections",count);
		session2.sendMsg(cntMsg);
	}
	else
	{
		sprintf(cntMsg,"You have successfully completed Mission III - Covert Ops with %d connections",count);
		session2.sendMsg(cntMsg);
	}
	file << cntMsg << endl;

	// Exit
	// -------
	session2.waitForEmptySendQ();

	prn("===="<<"Exiting Covert ops service with: "<<studentID.GetBuffer()<<" ====");
	studentID.ReleaseBuffer();
	session2.waitForRemoteClosure(10);

	file.close();
}

void
runServer()
{
	prn("Running Server");

	cListener listener;
	listener.startListening(20200);//30300);//20200);

	//cListener covertListener;
	//covertListener.startListening(20400);
	
	while (1)
	{
		if (!listener.isEmpty())
		{
			cConnectionData * conn = new cConnectionData();
			listener.pop(*conn);
		   _beginthread (runServerSession, 0, conn);
		}
		//if (!covertListener.isEmpty())
		//{
		//	cConnectionData * conn = new cConnectionData();
		//	covertListener.pop(*conn);
		//   _beginthread (runServerSessionCompletionPorts, 0, conn);
		//}
		Sleep(1);
	}
}
void
runClient(char * cmdLineIP=NULL)
{
	prn("Running Client");

	// Step 1: Get IP/port from the web using winInet api
	// --------------------------------------------------
	cHTTPMFC httpCon;
	CString * webPage = httpCon.openFile();

	if (!webPage)
	{
		prn("Couldn't find web page.  Exiting")
		return;
	}
	int offset = 0;
	CString ip = webPage->Tokenize(":\r\n ",offset);
	CString portStr = webPage->Tokenize(":\r\n ",offset);
	unsigned short port = atoi(portStr);
	//ip	= "12.230.194.193";
	//port	= 30300;//20200;

	// Step 2: Connect to server
	// -------------------------
	cSession session1;
	int error = session1.openTCP(ip.GetBuffer(),port);
	ip.ReleaseBuffer();
	if (error != 0)
		return;
	session1.run();

	// Step 3: Send UWNetID
	// --------------------
	session1.sendNetID("cwmail");

	// Step 4: Recv Instructions for next step
	// ---------------------------------------
	cPkt_ID * pkt = session1.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
		return;
	if (pkt->ID != kPKT_MESSAGE)
		return;
	cPkt_Message *msg = (cPkt_Message *)pkt;
	prn("Recv Msg: "<<&msg->instructions[1]);

	// Step 5: Start Listening on TCP
	// ------------------------------
	cListener listener;
	listener.startListening(40000);

	// Step 6: Send Server the address to connect to
	// if on a NAT, you will need to get your External IP
	// and set up port forwarding.
	// ---------------------------------------------
	if( cmdLineIP )
	{
		session1.sendAddress(cmdLineIP,40000);
	}
	else
	{
		session1.sendAddress(ip.GetBuffer(),40000);
		ip.ReleaseBuffer();
	}

	// Step 7: Wait for connection
	// ---------------------------
	cConnectionData conn;
	cSession * session2;
	while (1)
	{
		if (!listener.isEmpty())
		{
			listener.pop(conn);
			listener.stopListening();
			session2 = new cSession(&conn);
			session2->run();
			break;
		}
		Sleep(100);
	}

	// Step 8: Wait for furthur Instructions on TCP 2
	// ----------------------------------------------
	prn("====Wait for Instructions on TCP 2");
	pkt = session2->recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
		return;
	if (pkt->ID != kPKT_MESSAGE)
		return;
	msg = (cPkt_Message *)pkt;
	prn("====Recv Msg TCP 2: "<<&msg->instructions[1]);

	// Step 9: Start listening to UDP socket
	// -------------------------------------
	prn("====Listening on UDP port 40001");
	cSession session3;
	session3.openUDP_Recv(40001);

	// Step 10: Send address on TCP 2 for server to send UDP instructions to
	// --------------------------------------------------------------------
	if( cmdLineIP )
	{
		session2->sendAddress(cmdLineIP,40001);
	}
	else
	{
		session2->sendAddress(ip.GetBuffer(),40001);
		ip.ReleaseBuffer();
	}

	// Step 11: Recv on UDP port for Server's next Instructions
	// ------------------------------------------------------------------
	prn("====Waiting to recv UDP msg");
	pkt = session3.recvPkt(STANDARD_TIMEOUT);
	if (!pkt)
	{
		prn("====Exiting, recvPkt returned Null");
		return;
	}
	if (pkt->ID != kPKT_MESSAGE)
	{
		prn("====Exiting, Invalid pkt id: "<<pkt->ID<<" expecting: "<<kPKT_MESSAGE);
		return;
	}
	msg = (cPkt_Message *)pkt;
	prn("Recv Msg: "<<&msg->instructions[1]);

	// Step 12: Send Ack for instructions on UDP
	// -----------------------------------------
	while (!session1.recvPkt(1000))
	{
		if( cmdLineIP )
		{
			session3.sendUDP(cmdLineIP,20222,"ACK");
		}
		else
		{
			session3.sendUDP(ip.GetBuffer(),20222,"ACK");
			ip.ReleaseBuffer();
		}
	}
	// Step 13: Final instructions on TCP 2
	// ------------------------------------
	session1.recvPkt(STANDARD_TIMEOUT);	
	prn("Successfully finished");
}
int _tmain(int argc, _TCHAR* argv[])
{
	InitializeCriticalSection(&gPrintMutex);

	cWinsockInit winsockInit(2,2);
	if (argc==1)
		runServer();
	else if (argc==2)
		runClient();
	else
		runClient(argv[2]);

	DeleteCriticalSection(&gPrintMutex);
	return 0;
}
