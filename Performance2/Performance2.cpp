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
    while (true) {
            string line = b.get();
            if (!line.empty()) {
                if (line == "NULL") { break; }
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
    while (true) {
        string line = b.get();
        if (!line.empty()) {
            if (line == "NULL") { break; }
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
    int numberOfMultipleViews = 0;
    while (true) {
        string line = b.get();
        if (!line.empty()) {
            if (line == "NULL") { break; }
            auto ipStartTagBegin = line.find(ipStartTag);
            auto ipEndTagBegin = line.find(ipEndTag);
            auto ipAddressBegin = ipStartTagBegin + ipStartTag.length();
            string ipAddress(line, ipAddressBegin, ipEndTagBegin - ipAddressBegin);
            if (find(ipAddresses.begin(), ipAddresses.end(), ipAddress) == ipAddresses.end()) {
                ipAddresses.push_back(ipAddress);
            } else {
                ++numberOfMultipleViews;
            }
        }
    }
    return numberOfMultipleViews;
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

vector<string> constructLogJson(const vector<LogItem>& logData) {
    auto logDataSize = logData.size();
    vector<string> jsonData;
    jsonData.push_back("{\n  \"entries\": [\n");
    int n = 0;
    for (decltype(logDataSize) i = 0; i < logDataSize; ++i) {
        string outputJSON = "    {\n";
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
        outputJSON += "      ]\n    }";
        if (i != logDataSize - 1) {
            outputJSON += ",";
        }
        outputJSON += "\n";
        ++n;
    }
    jsonData.push_back("  ]\n}");
    cout << "Constructed " << n << " JSON lines should be 1251777\n";
    return jsonData;
}

vector<string> constructStatisticsJson(const vector<pair<string, float>>& sessions, const float& averageDuration, const int& views) {
    auto sessionsSize = sessions.size();
    vector<string> jsonData;
    jsonData.push_back("{\n  \"time_durations\": [\n");
    for (decltype(sessionsSize) i = 0; i < sessionsSize; ++i) {
        string outputJSON = "    {\n";
        outputJSON += "      \"session_id\": \"" + sessions[i].first + "\",\n";
        outputJSON += "      \"duration\": " + to_string(sessions[i].second) + "\n";
        outputJSON += "    }";
        if (i != sessionsSize - 1) {
            outputJSON += ",";
        }
        outputJSON += "\n";
        jsonData.push_back(outputJSON);
    }
    jsonData.push_back("  ],\n");
    jsonData.push_back("  \"average_duration\": " + to_string(averageDuration) + ",\n");
    jsonData.push_back("  \"multiple_views\": " + to_string(views) + "\n");
    jsonData.push_back("}");
    return jsonData;
}

void outputToFile(const vector<string>& json, const string& fileName) {
    ofstream jsonFile(fileName);
    for (string s : json) {
        jsonFile << s;
    }
    jsonFile.close();
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

        const string testFile = "log";
        ifstream xmlFile(testFile + ".xml");
        string line;
        circular_buffer<string> logLines(10000);
        circular_buffer<string> durationLines(10000);
        circular_buffer<string> addressLines(10000);
        future<vector<LogItem>> f1 = async(parseLogLines, ref(logLines));
        future<vector<pair<string, vector<string>>>> f2 = async(parseDurationLines, ref(durationLines));
        future<int> f3 = async(calculateNumberOfViews, ref(addressLines));
        // Parse XML file
        int i = 0;
        while (getline(xmlFile, line)) {
            logLines.put(line);
            durationLines.put(line);
            addressLines.put(line);
            ++i;
        }
        logLines.put("NULL");
        durationLines.put("NULL");
        addressLines.put("NULL");
        cout << "Processed " << i << " lines should be 1251777\n";
        xmlFile.close();

        vector<pair<string, vector<string>>> sessionTimes = f2.get();
        cout << "f2 complete\n";
        cout << "Processed " << sessionTimes.size() << " sessions should be 1251777\n";

        future<vector<pair<string, float>>> f4 = async(calculateDurations, ref(sessionTimes));
        future<float> f5 = async(calculateAverageDuration, ref(sessionTimes));

        vector<LogItem> logData = f1.get();
        cout << "f1 complete\n";
        cout << "Processed " << logData.size() << " log items should be 1251777\n";
        int views = f3.get();
        cout << "f3 complete\n";
        vector<pair<string, float>> sessionDurations = f4.get();
        cout << "f4 complete\n";
        float averageDuration = f5.get();
        cout << "f5 complete\n";

        future<vector<string>> f6 = async(constructLogJson, ref(logData));
        future<vector<string>> f7 = async(constructStatisticsJson, ref(sessionDurations), ref(averageDuration), ref(views));
        vector<string> logJson = f6.get();
        cout << "f6 complete\n";
        future<void> f8 = async(outputToFile, ref(logJson), testFile + ".json");
        vector<string> statsJson = f7.get();
        cout << "f7 complete\n";
        future<void> f9 = async(outputToFile, ref(statsJson), testFile + "_statistics.json");

        f8.get();
        cout << "f8 complete\n";
        f9.get();
        cout << "f9 complete\n";
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
