// Performance2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Performance2.h"

#include <algorithm>
#include <concurrent_queue.h>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>  
#include <iostream>
#include <istream>
#include <map>
#include <ppl.h>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
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

void outputToFile(const string& line, ofstream& file) {
    file << line;
}

string constructJsonLine(LogItem& item) {
    string outputJSON = "    {\n";
    outputJSON += R"(      "session_id": ")" + item.sessionId + "\",\n";
    outputJSON += R"(      "ip_address": ")" + item.ipAddress + "\",\n";
    outputJSON += R"(      "browser": ")" + item.browser + "\",\n";
    outputJSON += "      \"page_views\": [\n";
    for (auto i = item.pathTimes.begin(); i != item.pathTimes.end(); ++i) {
        outputJSON += "        {\n";
        outputJSON += R"(          "path": ")" + (*i).first + "\",\n";
        outputJSON += R"(          "time": ")" + (*i).second + "\"\n";
        outputJSON += "        }";
        if (i != item.pathTimes.end() - 1) {
            outputJSON += ",";
        }
        outputJSON += "\n";
    }
    outputJSON += "      ]\n    }";
    return outputJSON;
};

LogItem parseLogLine(const string& line) {
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
    LogItem item;
    auto sessionStartTagBegin = line.find(sessionStartTag);
    auto sessionEndTagBegin = line.find(sessionEndTag);
    auto sessionIdBegin = sessionStartTagBegin + sessionStartTag.length();
    string sessionId(line, sessionIdBegin, sessionEndTagBegin - sessionIdBegin);
    item.sessionId = move(sessionId);
    auto ipStartTagBegin = line.find(ipStartTag);
    auto ipEndTagBegin = line.find(ipEndTag);
    auto ipAddressBegin = ipStartTagBegin + ipStartTag.length();
    string ipAddress(line, ipAddressBegin, ipEndTagBegin - ipAddressBegin);
    item.ipAddress = move(ipAddress);
    auto browserStartTagBegin = line.find(browserStartTag);
    auto browserEndTagBegin = line.find(browserEndTag);
    auto browserBegin = browserStartTagBegin + browserStartTag.length();
    string browser(line, browserBegin, browserEndTagBegin - browserBegin);
    item.browser = move(browser);
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
        pathTimes.emplace_back(move(path), move(time));
        pos = timeEndTagBegin + timeEndTag.length();
        pathStartTagBegin = line.find(pathStartTag, pos);
    }
    item.pathTimes = move(pathTimes);
    return item;
}

atomic<bool> stopParsingLines(false);

void processLines(concurrency::concurrent_queue<string>& unProcessedLines) {
    concurrency::concurrent_queue<LogItem> logData;
    concurrency::concurrent_queue<LogItem> logDataCopy;
    concurrency::concurrent_queue<string> ipAddresses;
    atomic<bool> stopConstructingJson(false);
    atomic<bool> stopCalculatingViews(false);
    atomic<bool> stopCalculatingDurations(false);
    thread tParseLines([&stopConstructingJson, &stopCalculatingViews, &stopCalculatingDurations](concurrency::concurrent_queue<string>& unProcessedLines, concurrency::concurrent_queue<LogItem>& logData, concurrency::concurrent_queue<LogItem>& logDataCopy, concurrency::concurrent_queue<string>& ipAddresses) {
        while (true) {
            string line;
            const auto gotValue = unProcessedLines.try_pop(line);
            if (gotValue) {
                auto item = parseLogLine(line);
                ipAddresses.push(ref(item.ipAddress));
                logData.push(ref(item));
                logDataCopy.push(move(item));
            }
            else if (stopParsingLines && unProcessedLines.empty()) {
                stopConstructingJson = true;
                stopCalculatingViews = true;
                stopCalculatingDurations = true;
                break;
            }
        }
    }, ref(unProcessedLines), ref(logData), ref(logDataCopy), ref(ipAddresses));
    concurrency::concurrent_queue<string> logJson;
    atomic<bool> stopOutputtingToFile(false);
    thread tConstructLogJson([&stopConstructingJson, &stopOutputtingToFile](concurrency::concurrent_queue<LogItem>& logData, concurrency::concurrent_queue<string>& logJson) {
        bool firstValue = true;
        logJson.push("{\n  \"entries\": [\n");
        while (true) {
            LogItem item;
            const auto gotValue = logData.try_pop(item);
            if (gotValue) {
                string json;
                if (!firstValue) {
                    json += ",\n";
                }
                else {
                    firstValue = false;
                }
                json += constructJsonLine(item);
                logJson.push(move(json));
            }
            else if (stopConstructingJson && logData.empty()) {
                stopOutputtingToFile = true;
                break;
            }
        }
        logJson.push("\n  ]\n}");
    }, ref(logData), ref(logJson));
    concurrency::concurrent_queue<pair<string, int>> durations;
    atomic<bool> stopConstructingDurations(false);
    float averageDuration;
    thread tCalculateDurations([&stopCalculatingDurations, &stopConstructingDurations](concurrency::concurrent_queue<LogItem>& logData, concurrency::concurrent_queue<pair<string, int>>& durations, float& averageDuration) {
        auto i = 0;
        auto total = 0;
        while (true) {
            LogItem item;
            const auto gotValue = logData.try_pop(item);
            if (gotValue) {
                int duration;
                if (item.pathTimes.size() == 1) {
                    duration = 0;
                }
                else {
                    struct tm latestDateTm {};
                    istringstream(item.pathTimes[item.pathTimes.size() - 1].second) >> std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

                    struct tm oldestDateTm {};
                    istringstream(item.pathTimes[0].second) >> std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

                    const int differenceInSeconds = difftime(mktime(&latestDateTm), mktime(&oldestDateTm));
                    duration = move(differenceInSeconds);
                }
                durations.push({ item.sessionId, duration });
                total += duration;
                ++i;
            }
            else if (stopCalculatingDurations && logData.empty()) {
                stopConstructingDurations = true;
                averageDuration = total / i;
                break;
            }
        }
    }, ref(logDataCopy), ref(durations), ref(averageDuration));
    map<string, int> numberOfViews;
    atomic<bool> viewsCalculated(false);
    thread tCalculateViews([&stopCalculatingViews, &viewsCalculated](concurrency::concurrent_queue<string>& ipAddresses, map<string, int>& numberOfViews) {
        while (true) {
            string ipAddress;
            const auto gotValue = ipAddresses.try_pop(ipAddress);
            if (gotValue) {
                auto it = numberOfViews.find(ipAddress);
                if (it == numberOfViews.end()) {
                    numberOfViews.insert({ ipAddress, 1 });
                }
                else {
                    ++it->second;
                }
            }
            else if (stopCalculatingViews && ipAddresses.empty()) {
                viewsCalculated = true;
                break;
            }
        }
    }, ref(ipAddresses), ref(numberOfViews));
    concurrency::concurrent_queue<string> viewsJson;
    concurrency::concurrent_queue<string> durationsJson;
    atomic<bool> stopOutputtingViewsToFile(false);
    atomic<bool> stopOutputtingDurationsToFile(false);
    thread tConstructStatsJson([&stopConstructingDurations, &stopOutputtingViewsToFile, &stopOutputtingDurationsToFile, &viewsCalculated](map<string, int>& numberOfViews, concurrency::concurrent_queue<pair<string, int>>& durations, float& averageDuration, concurrency::concurrent_queue<string>& viewsJson, concurrency::concurrent_queue<string>& durationsJson) {
        thread tConstructViewsJson([&stopOutputtingViewsToFile, &stopOutputtingDurationsToFile, &viewsCalculated](map<string, int>& numberOfViews, concurrency::concurrent_queue<string>& viewsJson) {
            while (true) {
                if (viewsCalculated) {
                    boolean firstValue = true;
                    viewsJson.push("  \"multiple_views\": [\n");
                    for (const auto& v : numberOfViews) {
                        if (v.second > 1) {
                            if (!firstValue) {
                                viewsJson.push(",\n");
                            }
                            firstValue = false;
                            string json = "    {\n      \"ip_address\": \"";
                            json += v.first;
                            json += "\",\n      \"views\": ";
                            json += to_string(v.second);
                            json += "\n    }";
                            viewsJson.push(json);
                        }
                    }
                    viewsJson.push("\n  ]");
                    stopOutputtingViewsToFile = true;
                    break;
                }
            }
        }, ref(numberOfViews), ref(viewsJson));
        thread tConstructDurationsJson([&stopConstructingDurations, &stopOutputtingDurationsToFile](concurrency::concurrent_queue<pair<string, int>>& durations, float& averageDuration, concurrency::concurrent_queue<string>& durationsJson) {
            boolean firstValue = true;
            durationsJson.push("  \"durations\": [\n");
            while (true) {
                pair<string, int> p;
                const auto gotValue = durations.try_pop(p);
                if (gotValue) {
                    if (!firstValue) {
                        durationsJson.push(",\n");
                    }
                    firstValue = false;
                    string json = "    {\n      \"session_id\": \"";
                    json += p.first;
                    json += "\",\n      \"duration\": ";
                    json += to_string(p.second);
                    json += "\n    }";
                    durationsJson.push(json);
                }
                else if (stopConstructingDurations && durations.empty()) {
                    string json = "\n  ],\n";
                    json += "  \"average_duration\": ";
                    json += to_string(averageDuration);
                    durationsJson.push(json);
                    stopOutputtingDurationsToFile = true;
                    break;
                }
            }
        }, ref(durations), ref(averageDuration), ref(durationsJson));
        tConstructDurationsJson.join();
        tConstructViewsJson.join();
    }, ref(numberOfViews), ref(durations), ref(averageDuration), ref(viewsJson), ref(durationsJson));
    thread tOutputLog([&stopOutputtingToFile](concurrency::concurrent_queue<string>& logJson, const string& fileName) {
        ofstream jsonFile(fileName);
        while (true) {
            string json;
            const auto gotValue = logJson.try_pop(json);
            if (gotValue) {
                outputToFile(json, ref(jsonFile));
            }
            else if (stopOutputtingToFile && logJson.empty()) {
                break;
            }
        }
    }, ref(logJson), "log.json");
    thread tOutputStats([&stopOutputtingViewsToFile, &stopOutputtingDurationsToFile](concurrency::concurrent_queue<string>& viewsJson, concurrency::concurrent_queue<string>& durationsJson, const string& fileName) {
        ofstream jsonFile(fileName);
        jsonFile << "{\n";
        string startWith;
        bool firstValue = true;
        bool stopLooping = false;
        while (!stopLooping) {
            if (!durationsJson.empty()) {
                while (true) {
                    if (firstValue) {
                        outputToFile(startWith, jsonFile);
                        firstValue = false;
                    }
                    else {
                        string json;
                        const auto gotValue = durationsJson.try_pop(json);
                        if (gotValue) {
                            outputToFile(json, jsonFile);
                        }
                        else if (stopOutputtingDurationsToFile && durationsJson.empty()) {
                            startWith = ",\n";
                            stopLooping = true;
                            break;
                        }
                    }
                }
            }
        }
        firstValue = true;
        stopLooping = false;
        while (!stopLooping) {
            if (!viewsJson.empty()) {
                while (true) {
                    if (firstValue) {
                        outputToFile(startWith, jsonFile);
                        firstValue = false;
                    }
                    else {
                        string json;
                        const auto gotValue = viewsJson.try_pop(json);
                        if (gotValue) {
                            outputToFile(json, jsonFile);
                        }
                        else if (stopOutputtingViewsToFile && viewsJson.empty()) {
                            startWith = ",\n";
                            stopLooping = true;
                            break;
                        }
                    }
                }
            }
        }
        jsonFile << "\n}";
    }, ref(viewsJson), ref(durationsJson), "statistics.json");
    tParseLines.join();
    tConstructLogJson.join();
    tCalculateDurations.join();
    tCalculateViews.join();
    tConstructStatsJson.join();
    tOutputLog.join();
    tOutputStats.join();
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
        ios_base::sync_with_stdio(false);
        ifstream xmlFile("log.xml");
        string line;
        concurrency::concurrent_queue<string> q;
        thread t1(processLines, ref(q));
        while (getline(xmlFile, line)) {
            q.push(move(line));
        }
        stopParsingLines = true;
        xmlFile.close();
        t1.join();
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
