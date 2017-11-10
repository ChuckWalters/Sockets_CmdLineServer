// stdafx.h 
// Globally included system headers
// This file is the precompiled header
//
#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <afxinet.h>
#include <stdio.h>
#include <tchar.h>
#include <process.h> // _beginthread, _endthread
#include <iostream>
#include <queue>
#include <Winsock2.h>
#include <Ws2tcpip.h>

using namespace std;

extern CRITICAL_SECTION  gPrintMutex;
#define prn(a_prnstr_a){EnterCriticalSection(&gPrintMutex);{cout<<a_prnstr_a<<endl;}LeaveCriticalSection(&gPrintMutex);}
//#define prn(a_prnstr_a){}