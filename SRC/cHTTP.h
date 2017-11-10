#pragma once
#include "stdafx.h"

class cHTTPMFC
{
public:
	cHTTPMFC( char * sessionName = "HTTP Session" )
		:mSession(sessionName)
	{
		mPort	= 80;
		mFile	= NULL;
		mServer	= NULL;
	}
	~cHTTPMFC()
	{
		if (mFile)
			delete mFile;
		if (mServer)
			delete mServer;
	}
	bool
	connectToWebServer(CString serverName = "")
	{
		mServer = mSession.GetHttpConnection(serverName, mPort);
		if (mServer)
			return true;
		else
			return false;
	}
	CString *
	openFile(CString url = "http://courses.washington.edu/gamedev/C4/serveraddr.htm")
	{
		try
		{
			mFile = mSession.OpenURL(url);
			if (!mFile)
			{
				return NULL;
			}

			CString * str = new CString();
			char buff[1024];
			UINT nRead = 1;
			while (nRead > 0)
			{
				nRead = mFile->Read(buff, sizeof(buff));
				if (nRead > 0)
					str->Append(buff,nRead);
			}
			return str;
		}
		catch (CInternetException* pEx)
		{
			TCHAR msg[1024]; 
			pEx->GetErrorMessage(msg,sizeof(msg));
			prn(msg);
			return NULL;
		}
	}
private:
	CInternetSession mSession;
	CHttpConnection	*mServer;
	CStdioFile		*mFile;
	INTERNET_PORT	mPort;
};

// wininet version
class cHTTPWinInet
{
private:
	HINTERNET	mHTTPSessionHandle;
	HINTERNET	mURLHandle;

public:
	cHTTPWinInet() : 
		mHTTPSessionHandle( NULL ),
		mURLHandle( NULL)
	{
	}

	~cHTTPWinInet()
	{
		if( mURLHandle )
		{
			InternetCloseHandle( mURLHandle );
		}
		if( mHTTPSessionHandle )
		{
			InternetCloseHandle( mHTTPSessionHandle );
		}
	}

	CString *
	openFile( CString url = "http://courses.washington.edu/gamedev/C4/serveraddr.htm" )
	{
		mHTTPSessionHandle = InternetOpen( url, INTERNET_OPEN_TYPE_DIRECT, 0, 0, 0 );

		if ( !mHTTPSessionHandle )
		{
			return NULL;
		}

		mURLHandle = InternetOpenUrl(
			mHTTPSessionHandle, 
			url, 
			0, 
			0,
			INTERNET_FLAG_RELOAD, 
			0);

		if( mURLHandle == 0 )
		{
			TCHAR str[2048];
			DWORD errNum;
			DWORD strLen = 2048;
			BOOL err = InternetGetLastResponseInfo( &errNum, str, &strLen);

			return NULL;
		}

		DWORD bytesToRead;
		if( !InternetQueryDataAvailable( mURLHandle, &bytesToRead, 0, 0 ) )
		{
			return NULL;
		}

		CString * str = new CString();
		char buff[1024];
		while( bytesToRead > 0 )
		{
			DWORD bytesRead=0;

			if( !InternetReadFile( mURLHandle, buff, sizeof(buff), &bytesRead ) )
			{
				return NULL;
			}
			bytesToRead -= bytesRead;
			if( bytesRead > 0 )
			{
				str->Append( buff, bytesRead );
			}
		}
		return str;
	}
};
