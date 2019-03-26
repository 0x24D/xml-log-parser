// Performance2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Performance2.h"

#include <algorithm>
#include <cmath>
#include <concurrent_queue.h>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>  
#include <iostream>
#include <istream>
#include <iterator>
#include <numeric>
#include <ppl.h>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

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
    deque<pair<string, string>> pathTimes;
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

void outputToFile(concurrency::concurrent_queue<string>& q, const string& fileName) {
    ofstream jsonFile(fileName);
    while (true) {
        string line;
        auto popped = q.try_pop(line);
        if (popped) {
            if (line == "NULL") { break; }
            jsonFile << line;
        }
    }
    jsonFile.close();
}

deque<LogItem> parseLogLines(concurrency::concurrent_queue<string>& q) {
    deque<LogItem> logData;
    while (true) {
        string line;
        auto popped = q.try_pop(line);
        if (popped) {
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
            deque<pair<string, string>> pathTimes;
            while (pathStartTagBegin != string::npos) {
                auto pathEndTagBegin = line.find(pathEndTag, pos);
                auto pathBegin = pathStartTagBegin + pathStartTag.length();
                string path(line, pathBegin, pathEndTagBegin - pathBegin);
                auto timeStartTagBegin = line.find(timeStartTag, pos);
                auto timeEndTagBegin = line.find(timeEndTag, pos);
                auto timeBegin = timeStartTagBegin + timeStartTag.length();
                string time(line, timeBegin, timeEndTagBegin - timeBegin);
                pathTimes.push_back({ path, time });
                pos = timeEndTagBegin + timeEndTag.length();
                pathStartTagBegin = line.find(pathStartTag, pos);
            }
            item.pathTimes = pathTimes;
            logData.push_back(move(item));
        }
    }
    return logData;
}

int calculateNumberOfViews(const deque<LogItem>& logData) {
    deque<string> ipAddresses;
    int numberOfMultipleViews = 0;
    for (auto item : logData) {
        if (find(ipAddresses.begin(), ipAddresses.end(), item.ipAddress) == ipAddresses.end()) {
            ipAddresses.push_back(item.ipAddress);
        }
        else {
            ++numberOfMultipleViews;
        }
    }
    return numberOfMultipleViews;
}

deque<pair<string, float>> calculateDurations(const deque<LogItem>& logData) {
    deque<pair<string, float>> durations;
    for (auto item : logData) {
        float duration;
        if (item.pathTimes.size() == 1) {
            duration = 0.0f;
        }
        else {
            struct tm latestDateTm;
            istringstream(item.pathTimes[item.pathTimes.size() - 1].second) >> std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

            struct tm oldestDateTm;
            istringstream(item.pathTimes[0].second) >> std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

            float differenceInSeconds = difftime(mktime(&latestDateTm), mktime(&oldestDateTm));
            duration = differenceInSeconds;
        }
        durations.push_back({ item.sessionId, duration });
    }
    return durations;
}

float calculateAverageDuration(const deque<pair<string, float>>& sessionDurations) {
    float totalDuration = 0.0f;
    for (auto session : sessionDurations) {
        totalDuration += session.second;
    }
    return totalDuration / sessionDurations.size();
}

void constructAndOutputLogJson(const deque<LogItem>& logData, const string& fileName) {
    concurrency::concurrent_queue<string> jsonData;
    future<void> f = async(outputToFile, ref(jsonData), ref(fileName));
    jsonData.push("{\n  \"entries\": [\n");
    for (auto i = logData.begin(); i != logData.end(); ++i) {
        string outputJSON = "    {\n";
        outputJSON += "      \"session_id\": \"" + (*i).sessionId + "\",\n";
        outputJSON += "      \"ip_address\": \"" + (*i).ipAddress + "\",\n";
        outputJSON += "      \"browser\": \"" + (*i).browser + "\",\n";
        outputJSON += "      \"page_views\": [\n";
        for (auto j = (*i).pathTimes.begin(); j != (*i).pathTimes.end(); ++j) {
            outputJSON += "        {\n";
            outputJSON += "          \"path\": \"" + (*j).first + "\",\n";
            outputJSON += "          \"time\": \"" + (*j).second + "\"\n";
            outputJSON += "        }";
            if (j != (*i).pathTimes.end() - 1) {
                outputJSON += ",";
            }
            outputJSON += "\n";
        }
        outputJSON += "      ]\n    }";
        if (i != logData.end() - 1) {
            outputJSON += ",";
        }
        outputJSON += "\n";
        jsonData.push(move(outputJSON));
    }
    jsonData.push("  ]\n}");
    jsonData.push("NULL");
    f.get();
}

void constructAndOutputStatisticsJson(const deque<pair<string, float>>& sessions, const float& averageDuration, const int& views, const string& fileName) {
    concurrency::concurrent_queue<string> jsonData;
    future<void> f = async(outputToFile, ref(jsonData), ref(fileName));
    jsonData.push("{\n  \"time_durations\": [\n");
    for (auto i = sessions.begin(); i != sessions.end(); ++i) {
        string outputJSON = "    {\n";
        outputJSON += "      \"session_id\": \"" + (*i).first + "\",\n";
        outputJSON += "      \"duration\": " + to_string((*i).second) + "\n";
        outputJSON += "    }";
        if (i != sessions.end() - 1) {
            outputJSON += ",";
        }
        outputJSON += "\n";
        jsonData.push(move(outputJSON));
    }
    jsonData.push("  ],\n");
    jsonData.push("  \"average_duration\": " + to_string(averageDuration) + ",\n");
    jsonData.push("  \"multiple_views\": " + to_string(views) + "\n");
    jsonData.push("}");
    jsonData.push("NULL");
    f.get();
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
        concurrency::concurrent_queue<string> queue;
        future<deque<LogItem>> f1D1 = async(launch::async, parseLogLines, ref(queue));
        future<deque<LogItem>> f1D2 = async(launch::async, parseLogLines, ref(queue));
        future<deque<LogItem>> f1D3 = async(launch::async, parseLogLines, ref(queue));
        // Parse XML file
        int i = 0;
        while (getline(xmlFile, line)) {
            queue.push(line);
            ++i;
        }
        // push a NULL string for each future
        queue.push("NULL");
        queue.push("NULL");
        queue.push("NULL");
        xmlFile.close();

        deque<LogItem> logData(move(f1D1.get()));
        deque<LogItem> logData2 = f1D2.get();
        logData.insert(logData.end(), make_move_iterator(logData2.begin()), make_move_iterator(logData2.end()));
        deque<LogItem> logData3 = f1D3.get();
        logData.insert(logData.end(), make_move_iterator(logData3.begin()), make_move_iterator(logData3.end()));
        cout << "f1 complete\n";

        future<deque<pair<string, float>>> f3 = async(launch::async, calculateDurations, ref(logData));
        future<int> f5 = async(launch::async, calculateNumberOfViews, ref(logData));

        deque<pair<string, float>> durations = f3.get();
        cout << "f3 complete\n";

        future<float> f4 = async(launch::async, calculateAverageDuration, ref(durations));
        float averageDuration = f4.get();
        cout << "f4 complete\n";

        //future<void> f2 = async(launch::async, constructAndOutputLogJson, ref(logData), testFile + ".json");

        int views = f5.get();
        cout << "f5 complete\n";
        
        future<void> f6 = async(launch::async, constructAndOutputStatisticsJson, ref(durations), ref(averageDuration), ref(views), testFile + "_statistics.json");
        f6.get();
        cout << "f6 complete\n";

        //f2.get();
        //cout << "f2 complete\n";
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
