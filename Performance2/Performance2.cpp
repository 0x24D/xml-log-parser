// Performance2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Performance2.h"

#include <algorithm>
#include <array>
#include "circular_buffer.cpp"
#include <cmath>
#include <ctime>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>  
#include <iostream>
#include <istream>
#include <numeric>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
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
boolean logsRead(false);

class XMLParser {
public:
    string constructLogJson(const vector<LogItem>& logData) {
        auto logDataSize = logData.size();
        string outputJSON = "{\n";
        for (decltype(logDataSize) i = 0; i < logDataSize; ++i) {
            outputJSON += "  \"entries\": [\n    {\n";
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
     string constructStatisticsJson(const vector<string>& sessionIds, const vector<float>& durations, const float& averageDuration) {
         auto sessionsSize = sessionIds.size();
        string outputJSON = "{\n";
        outputJSON += "  \"time_durations\": [\n";
        for (decltype(sessionsSize) i = 0; i < sessionsSize; ++i) {
            outputJSON += "    {\n";
            outputJSON += "      \"session_id\": \"" + sessionIds[i] + "\",\n";
            outputJSON += "      \"duration\": " + to_string(durations[i]) + "\n";
            outputJSON += "    }";
            if (i != sessionsSize - 1) {
                outputJSON += ",";
            }
            outputJSON += "\n";
        }
        outputJSON += "  ],\n";
        outputJSON += "  \"average_duration\": " + to_string(averageDuration) + "\n";
        outputJSON += "}";
        return outputJSON;
    }
    void outputToFile(const string& json, const string& fileName) {
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

vector<LogItem> parseLogLines(circular_buffer<string>& b) {
    vector<LogItem> logData;
    while (!logsRead || !b.empty()) {
        if (!b.empty()) {
            string line = b.get();
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
            logData.push_back(item);
        }
    }
    return logData;
}
vector<pair<string, vector<string>>> parseDurationLines(circular_buffer<string>& b) {
    vector<pair<string, vector<string>>> pairs;
    while (!logsRead || !b.empty()) {
        if (!b.empty()) {
            string line = b.get();
            auto sessionStartTagBegin = line.find(sessionStartTag);
            auto sessionEndTagBegin = line.find(sessionEndTag);
            auto sessionIdBegin = sessionStartTagBegin + sessionStartTag.length();
            string sessionId(line, sessionIdBegin, sessionEndTagBegin - sessionIdBegin);
            vector<string> times;
            size_t pos = 0;
            auto pathStartTagBegin = line.find(pathStartTag);
            while (pathStartTagBegin != string::npos) {
                auto timeStartTagBegin = line.find(timeStartTag, pos);
                auto timeEndTagBegin = line.find(timeEndTag, pos);
                auto timeBegin = timeStartTagBegin + timeStartTag.length();
                string time(line, timeBegin, timeEndTagBegin - timeBegin);
                times.push_back(time);
                pos = timeEndTagBegin + timeEndTag.length();
                pathStartTagBegin = line.find(pathStartTag, pos);
            }
            pairs.push_back({ sessionId, times });
        }
    }
    return pairs;
}

int calculateNumberOfViews(circular_buffer<string>& b) {
    vector<string> ipAddresses;
    int numberOfDuplicateViews = 0;
    while (!logsRead || !b.empty()) {
        if (!b.empty()) {
            string line = b.get();
            auto ipStartTagBegin = line.find(ipStartTag);
            auto ipEndTagBegin = line.find(ipEndTag);
            auto ipAddressBegin = ipStartTagBegin + ipStartTag.length();
            string ipAddress(line, ipAddressBegin, ipEndTagBegin - ipAddressBegin);
            if (find(ipAddresses.begin(), ipAddresses.end(), ipAddress) == ipAddresses.end()) {
                ipAddresses.push_back(ipAddress);
            } else {
                ++numberOfDuplicateViews;
            }
        }
    }
    return numberOfDuplicateViews;
}

vector<pair<string, float>> calculateDurations(const vector<pair<string, vector<string>>>& sessionTimes) {
    vector<pair<string, float>> durations;
    for (pair<string, vector<string>> pair : sessionTimes) {
        float duration;
        if (pair.second.size() == 1) {
            duration = 0.0f;
        }
        else {
            struct tm latestDateTm;
            istringstream(pair.second[pair.second.size() - 1]) >> std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

            struct tm oldestDateTm;
            istringstream(pair.second[0]) >> std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

            float differenceInSeconds = difftime(mktime(&latestDateTm), mktime(&oldestDateTm));
            duration = differenceInSeconds;
        }
        durations.push_back({ pair.first, duration });
    }
    return durations;
}

float calculateAverageDuration(const vector<pair<string, vector<string>>>& sessionTimes) {
    vector<float> durations;
    for (pair<string, vector<string>> pair : sessionTimes) {
        float duration;
        if (pair.second.size() == 1) {
            duration = 0.0f;
        }
        else {
            struct tm latestDateTm;
            istringstream(pair.second[pair.second.size() - 1]) >> std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

            struct tm oldestDateTm;
            istringstream(pair.second[0]) >> std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

            float differenceInSeconds = difftime(mktime(&latestDateTm), mktime(&oldestDateTm));
            duration = differenceInSeconds;
        }
        durations.push_back(duration);
    }
    auto totalDuration = accumulate(durations.begin(), durations.end(), 0.0);
    return totalDuration / durations.size();
}

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

        const string testFile = "testdata\\2";
        ifstream xmlFile(testFile + ".xml");
        string line;
        circular_buffer<string> logLines(10);
        circular_buffer<string> durationLines(10);
        circular_buffer<string> addressLines(10);
        future<vector<LogItem>> f1 = async(parseLogLines, ref(logLines));
        future<vector<pair<string, vector<string>>>> f2 = async(parseDurationLines, ref(durationLines));
        future<int> f3 = async(calculateNumberOfViews, ref(addressLines));
        // Parse XML file
        while (getline(xmlFile, line)) {
            {
                logLines.put(line);
                durationLines.put(line);
                addressLines.put(line);
            }
        }
        logsRead = true;
        xmlFile.close();

        vector<pair<string, vector<string>>> sessionTimes = f2.get();

        future<vector<pair<string, float>>> f4 = async(calculateDurations, ref(sessionTimes));
        future<float> f5 = async(calculateAverageDuration, ref(sessionTimes));
        vector<pair<string, float>> sessionDurations = f4.get();
        float averageDuration = f5.get();
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
