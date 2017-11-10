#pragma once
#include "Winsock2.h"

//#pragma message( "================================================================================" )
//#pragma message( "== Hello there game developers!  These warnings show the #pragma pack at work ==" )
//#pragma message( "================================================================================" )

//#pragma pack (show) // shows defaut packing (information only)
#pragma pack (1) // set to byte packing
//#pragma pack (show) // shows that byte packing has been set (information only)

///
/// This is the packet stored in the queue
///
class cPkt
{
public:
	short	mLen;	// packet length, including this header
	// Add special packetLayer protocols
	// Encryption
	// Compression
	// ...
};
// Presentation Layer
//====================================================================
// Application Layer

typedef enum tPktID
{
	kPKT_STUDENTINFO=0,
	kPKT_MESSAGE,
	kPKT_ADDRESS,
	kPKT_STATUS,
};
class cPkt_ID
{
public:
	char	ID; // every packet must have a unique ID
};

class cPkt_StudentInfo : public cPkt_ID
{
public:
	char	name[32];	// Must be null terminated (not compressed)
};

class cPkt_Message : public cPkt_ID
{
public:
	char	instructions[256];   // compressed, first 8bits define length used (null terminator not passed over net 
};

class cPkt_Address : public cPkt_ID
{
public:
	char	address[16];	// Must be null terminated (not compressed)
	u_short	port;
};

class cPkt_Status : public cPkt_ID
{
public:
	char	status[32];	// Must be null terminated (not compressed)
};

#pragma pack () // set to default byte packing
//#pragma pack (show) // shows defaut packing reset, should be same as first show (information only)