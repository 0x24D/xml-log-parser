// Performance2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Performance2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>  
#include <iostream>
#include <istream>
#include <sstream>
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

struct LogItem {
    string sessionId;
    string ipAddress;
    string browser;
    vector<string> paths;
    vector<string> times;
};

class XMLParser {
public:
    LogItem parseLogLine(string line) {
        LogItem item;
        auto sessionStartTagBegin = line.find(sessionStartTag);
        auto sessionEndTagBegin = line.find(sessionEndTag);
        auto sessionIdBegin = sessionStartTagBegin + sessionStartTag.length();
        string sessionId(line, sessionIdBegin, sessionEndTagBegin - sessionIdBegin);
        item.sessionId = sessionId;
        auto ipStartTagBegin = line.find(ipStartTag);
        auto ipEndTagBegin = line.find(ipEndTag);
        auto ipAddressBegin = ipStartTagBegin + ipStartTag.length();
        string ipAddress(line, ipAddressBegin, ipEndTagBegin - ipAddressBegin);
        item.ipAddress = ipAddress;
        auto browserStartTagBegin = line.find(browserStartTag);
        auto browserEndTagBegin = line.find(browserEndTag);
        auto browserBegin = browserStartTagBegin + browserStartTag.length();
        string browser(line, browserBegin, browserEndTagBegin - browserBegin);
        item.browser = browser;
        size_t pos = 0;
        auto pathStartTagBegin = line.find(pathStartTag);
        vector<string> paths;
        vector<string> times;
        while (pathStartTagBegin != string::npos) {
            auto pathEndTagBegin = line.find(pathEndTag, pos);
            auto pathBegin = pathStartTagBegin + pathStartTag.length();
            string path(line, pathBegin, pathEndTagBegin - pathBegin);
            paths.push_back(path);
            auto timeStartTagBegin = line.find(timeStartTag, pos);
            auto timeEndTagBegin = line.find(timeEndTag, pos);
            auto timeBegin = timeStartTagBegin + timeStartTag.length();
            string time(line, timeBegin, timeEndTagBegin - timeBegin);
            times.push_back(time);
            pos = timeEndTagBegin + timeEndTag.length();
            pathStartTagBegin = line.find(pathStartTag, pos);
        }
        item.paths = paths;
        item.times = times;
        return item;
    }
    string constructLogJson(vector<LogItem> logData) {
        auto logDataSize = logData.size();
        string outputJSON = "{\n";
        for (decltype(logDataSize) i = 0; i < logDataSize; ++i) {
            outputJSON += "  \"entry\": [\n    {\n";
            outputJSON += "      \"session_id\": \"" + logData[i].sessionId + "\",\n";
            outputJSON += "      \"ip_address\": \"" + logData[i].ipAddress + "\",\n";
            outputJSON += "      \"browser\": \"" + logData[i].browser + "\",\n";
            outputJSON += "      \"page_views\": [\n";
            auto pathsSize = logData[i].paths.size();
            for (decltype(pathsSize) j = 0; j < pathsSize; ++j) {
                outputJSON += "        {\n";
                outputJSON += "          \"path\": \"" + logData[i].paths[j] + "\",\n";
                outputJSON += "          \"time\": \"" + logData[i].times[j] + "\"\n";
                outputJSON += "        }";
                if (j != pathsSize - 1) {
                    outputJSON += ",";
                }
                outputJSON += "\n";
            }
            outputJSON += "      ]\n    }\n  ]";
            if (i != logDataSize - 1) {
                outputJSON += ",";
            }
            outputJSON += "\n";
        }
        outputJSON += "}";
        return outputJSON;
    }
    void outputToFile(string fileName, string json) {
        ofstream jsonFile(fileName);
        jsonFile << json;
        jsonFile.close();
    }
private:
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
};

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
        
        XMLParser parser;

        const string testFile = "testdata\\2";
        ifstream xmlFile(testFile + ".xml");
        string line;
        vector<LogItem> logData;
        // Parse XML file
        while (getline(xmlFile, line)) {
            logData.push_back(parser.parseLogLine(line));
        }
        xmlFile.close();
        // Calculate time per session
        vector<string> sessionIds;
        vector<string> durations;
        for (LogItem item : logData) {
            sessionIds.push_back(item.sessionId);
            if (item.times.size() == 1) {
                durations.push_back("00:00:00");
            } else {
                struct tm latestDateTm;
                istringstream(item.times[item.times.size() - 1]) >> std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

                struct tm oldestDateTm;
                istringstream(item.times[0]) >> std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

                int differenceInSeconds = difftime(mktime(&latestDateTm), mktime(&oldestDateTm));

                auto hour = differenceInSeconds / 3600;
                auto minute = (differenceInSeconds % 3600) / 60;
                auto second = differenceInSeconds % 60;
                ostringstream hourStream;
                hourStream << std::setw(2) << std::setfill('0') << hour;
                ostringstream minuteStream;
                minuteStream << std::setw(2) << std::setfill('0') << minute;
                ostringstream secondStream;
                secondStream << std::setw(2) << std::setfill('0') << second;
                durations.push_back(hourStream.str() + ":" + minuteStream.str() + ":" + secondStream.str());
            }
        }
        for (string duration : durations) {
            cout << duration << "\n";
        }
        // Construct JSON file
        //string outputJson = parser.constructLogJson(logData);
        //
        //// Output to .json file
        //parser.outputToFile(testFile + ".json", outputJson);

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
