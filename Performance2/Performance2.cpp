// Performance2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Performance2.h"

#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Timer - used to established precise timings for code.
class TIMER
{
	LARGE_INTEGER t_;

	__int64 current_time_;

	public:
		TIMER()	// Default constructor. Initialises this timer with the current value of the hi-res CPU timer.
		{
			QueryPerformanceCounter(&t_);
			current_time_ = t_.QuadPart;
		}

		TIMER(const TIMER &ct)	// Copy constructor.
		{
			current_time_ = ct.current_time_;
		}

		TIMER& operator=(const TIMER &ct)	// Copy assignment.
		{
			current_time_ = ct.current_time_;
			return *this;
		}

		TIMER& operator=(const __int64 &n)	// Overloaded copy assignment.
		{
			current_time_ = n;
			return *this;
		}

		~TIMER() {}		// Destructor.

		static __int64 get_frequency()
		{
			LARGE_INTEGER frequency;
			QueryPerformanceFrequency(&frequency); 
			return frequency.QuadPart;
		}

		__int64 get_time() const
		{
			return current_time_;
		}

		void get_current_time()
		{
			QueryPerformanceCounter(&t_);
			current_time_ = t_.QuadPart;
		}

		inline bool operator==(const TIMER &ct) const
		{
			return current_time_ == ct.current_time_;
		}

		inline bool operator!=(const TIMER &ct) const
		{
			return current_time_ != ct.current_time_;
		}

		__int64 operator-(const TIMER &ct) const		// Subtract a TIMER from this one - return the result.
		{
			return current_time_ - ct.current_time_;
		}

		inline bool operator>(const TIMER &ct) const
		{
			return current_time_ > ct.current_time_;
		}

		inline bool operator<(const TIMER &ct) const
		{
			return current_time_ < ct.current_time_;
		}

		inline bool operator<=(const TIMER &ct) const
		{
			return current_time_ <= ct.current_time_;
		}

		inline bool operator>=(const TIMER &ct) const
		{
			return current_time_ >= ct.current_time_;
		}
};

CWinApp theApp;  // The one and only application object

using namespace std;



int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	// initialize Microsoft Foundation Classes, and print an error if failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// Application starts here...

		// Time the application's execution time.
		TIMER start;	// DO NOT CHANGE THIS LINE. Timing will start here.

		//--------------------------------------------------------------------------------------
		// Insert your code from here...
        struct logItem {
            string sessionId;
            string ipAddress;
            string browser;
            vector<string> paths;
            vector<string> times;
        };
        const string sessionStartTag = "<sessionid>";
        const string sessionEndTag = "</sessionid>";
        const string ipStartTag = "<ipaddress>";
        const string ipEndTag = "</ipaddress>";
        const string browserStartTag = "<browser>";
        const string browserEndTag = "</browser>";
        const string pathStartTag = "<path>";
        const string pathEndTag = "</path>";
        const string timeStartTag = "<time>";
        const string timeEndTag = "</time>";

        ifstream xmlFile("testdata\\2.xml");
        string line;
        vector<logItem> logData;
        while (getline(xmlFile, line)) {
            logItem item;
            // sessionid
            auto sessionStartTagBegin = line.find(sessionStartTag);
            auto sessionEndTagBegin = line.find(sessionEndTag);
            auto sessionIdBegin = sessionStartTagBegin + sessionStartTag.length();
            string sessionId(line, sessionIdBegin, sessionEndTagBegin - sessionIdBegin);
            cout << sessionId << '\n';
            item.sessionId = sessionId;
            // ipaddress
            auto ipStartTagBegin = line.find(ipStartTag);
            auto ipEndTagBegin = line.find(ipEndTag);
            auto ipAddressBegin = ipStartTagBegin + ipStartTag.length();
            string ipAddress(line, ipAddressBegin, ipEndTagBegin - ipAddressBegin);
            cout << ipAddress << '\n';
            item.ipAddress = ipAddress;
            // browser
            auto browserStartTagBegin = line.find(browserStartTag);
            auto browserEndTagBegin = line.find(browserEndTag);
            auto browserBegin = browserStartTagBegin + browserStartTag.length();
            string browser(line, browserBegin, browserEndTagBegin - browserBegin);
            cout << browser << '\n';
            item.browser = browser;
            // multiple - path: time
            int pos = 0;
            auto pathStartTagBegin = line.find(pathStartTag);
            vector<string> paths;
            vector<string> times;
            while (pathStartTagBegin != string::npos) {
                auto pathEndTagBegin = line.find(pathEndTag, pos);
                auto pathBegin = pathStartTagBegin + pathStartTag.length();
                string path(line, pathBegin, pathEndTagBegin - pathBegin);
                cout << path << '\n';
                paths.push_back(path);
                auto timeStartTagBegin = line.find(timeStartTag, pos);
                auto timeEndTagBegin = line.find(timeEndTag, pos);
                auto timeBegin = timeStartTagBegin + timeStartTag.length();
                string time(line, timeBegin, timeEndTagBegin - timeBegin);
                cout << time << '\n';
                times.push_back(time);
                pos = timeEndTagBegin + timeEndTag.length();
                pathStartTagBegin = line.find(pathStartTag, pos);
            }
            item.paths = paths;
            item.times = times;
            logData.push_back(item);
        }

		//-------------------------------------------------------------------------------------------------------
		// How long did it take?...   DO NOT CHANGE FROM HERE...
		
		TIMER end;

		TIMER elapsed;
		
		elapsed = end - start;

		__int64 ticks_per_second = start.get_frequency();

		// Display the resulting time...

		double elapsed_seconds = (double)elapsed.get_time() / (double)ticks_per_second;

		cout << "Elapsed time (seconds): " << elapsed_seconds;
		cout << endl;
		cout << "Press a key to continue" << endl;

		char c;
		cin >> c;
	}

	return nRetCode;
}
