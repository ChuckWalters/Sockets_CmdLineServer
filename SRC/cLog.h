#pragma once
#include "cSmartPtr.h"
#include <iostream>
#include <time.h>
#include <fstream>
using namespace std;

	class Log : 
		public std::ofstream,
		public cCriticalSection
	{
	public:
		Log(const char * logName):ofstream(logName,ios_base::out|ios_base::app)
		{
			Lock();
			*this<<"============================="<<endl;
			timeStamp();
			UnLock();
		}
		virtual
		~Log()
		{
		}
		void
		timeStamp()
		{
			struct tm *newtime;
			__time64_t long_time;
			_time64( &long_time );                /* Get time as long integer. */
			newtime = _localtime64( &long_time ); /* Convert to local time. */
			Lock();
			*this << asctime( newtime )<<endl;
			UnLock();
		}
		void
			hexDump(char * data, int bytes, int bytesPerLine=20)
		{
			int i,w;

			Lock();

			for (i=0,w=0; i<bytes; i++,w++)
			{
				char str[5];
				if (w > bytesPerLine)
				{
					*this << endl;
					w=0;
				}
				sprintf(str," %2X",data[i]);
				*this <<str;
			}
			*this << endl;
			UnLock();
		}

		//friend ostream& operator<< ( ostream& os, cLog& log );

	};
