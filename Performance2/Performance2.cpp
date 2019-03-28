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
#include <map>
#include <mutex>
#include <numeric>
#include <ppl.h>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
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
    outputJSON += "      \"session_id\": \"" + item.sessionId + "\",\n";
    outputJSON += "      \"ip_address\": \"" + item.ipAddress + "\",\n";
    outputJSON += "      \"browser\": \"" + item.browser + "\",\n";
    outputJSON += "      \"page_views\": [\n";
    for (auto i = item.pathTimes.begin(); i != item.pathTimes.end(); ++i) {
        outputJSON += "        {\n";
        outputJSON += "          \"path\": \"" + (*i).first + "\",\n";
        outputJSON += "          \"time\": \"" + (*i).second + "\"\n";
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
        pathTimes.push_back({ move(path), move(time) });
        pos = timeEndTagBegin + timeEndTag.length();
        pathStartTagBegin = line.find(pathStartTag, pos);
    }
    item.pathTimes = move(pathTimes);
    return item;
}

bool stopParsingLines(false);
bool stopConstructingJson(false);
bool stopOutputtingToFile(false);
bool stopCalculatingViews(false);
bool stopCalculatingDurations(false);
bool stopOutputtingViewsToFile(false);
bool stopOutputtingDurationsToFile(false);

bool durationsCalculated(false);
bool averageDurationCalculated(false);
bool viewsCalculated(false);

const string testFile = "testdata\\2";

void processLines(concurrency::concurrent_queue<string>& unProcessedLines) {
    concurrency::concurrent_queue<LogItem> logData;
    concurrency::concurrent_queue<LogItem> logDataCopy;
    concurrency::concurrent_queue<string> ipAddresses;
    future<void> fParseLines = async(launch::async, [](concurrency::concurrent_queue<string>& unProcessedLines, concurrency::concurrent_queue<LogItem>& logData, concurrency::concurrent_queue<LogItem>& logDataCopy, concurrency::concurrent_queue<string>& ipAddresses) {
        int i = 0;
        while (true) {
            string line;
            auto gotValue = unProcessedLines.try_pop(line);
            if (gotValue) {
                LogItem item = parseLogLine(move(line));
                ipAddresses.push(ref(item.ipAddress));
                logData.push(ref(item));
                logDataCopy.push(move(item));
                ++i;
            }
            else if (stopParsingLines && unProcessedLines.empty()) {
                cout << "Parsed " << i << " lines\n";
                stopConstructingJson = true;
                stopCalculatingViews = true;
                stopCalculatingDurations = true;
                break;
            }
        }
    }, ref(unProcessedLines), ref(logData), ref(logDataCopy), ref(ipAddresses));
    concurrency::concurrent_queue<string> logJson;
    future<void> fConstructLogJson = async(launch::async, [](concurrency::concurrent_queue<LogItem>& logData, concurrency::concurrent_queue<string>& logJson) {
        bool firstValue = true;
        logJson.push("{\n  \"entries\": [\n");
        int i = 0;
        while (true) {
            LogItem item;
            auto gotValue = logData.try_pop(item);
            if (gotValue) {
                string json = "";
                if (!firstValue) {
                    json += ",\n";
                }
                else {
                    firstValue = false;
                }
                json += constructJsonLine(item);
                logJson.push(move(json));
                ++i;
            }
            else if (stopConstructingJson && logData.empty()) {
                cout << "Constructed " << i << " json lines\n";
                stopOutputtingToFile = true;
                break;
            }
        }
        logJson.push("\n  ]\n}");
    }, ref(logData), ref(logJson));
    unordered_map<string, float> durations;
    float averageDuration;
    future<void> fCalculateDurations = async(launch::async, [](concurrency::concurrent_queue<LogItem>& logData, unordered_map<string, float>& durations, float& averageDuration) {
        int i = 0;
        while (true) {
            LogItem item;
            float total = 0.0f;
            auto gotValue = logData.try_pop(item);
            if (gotValue) {
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
                durations.insert({ item.sessionId, duration });
                total += duration;
                ++i;
            }
            else if (stopCalculatingDurations && logData.empty()) {
                durationsCalculated = true;
                averageDuration = total / i;
                averageDurationCalculated = true;
                cout << "Calculated " << i << " durations, average is " << averageDuration << "\n";
                break;
            }
        }
    }, ref(logDataCopy), ref(durations), ref(averageDuration));
    map<string, int> numberOfViews;
    future<void> fCalculateViews = async(launch::async, [](concurrency::concurrent_queue<string>& ipAddresses, map<string, int>& numberOfViews) {
        int i = 0;
        while (true) {
            string ipAddress;
            auto gotValue = ipAddresses.try_pop(ipAddress);
            if (gotValue) {
                auto it = numberOfViews.find(ipAddress);
                if (it == numberOfViews.end()) {
                    numberOfViews.insert({ ipAddress, 1 });
                }
                else {
                    ++it->second;
                }
                ++i;
            }
            else if (stopCalculatingViews && ipAddresses.empty()) {
                viewsCalculated = true;
                cout << "Calculated " << i << " views\n";
                break;
            }
        }
    }, ref(ipAddresses), ref(numberOfViews));
    concurrency::concurrent_queue<string> viewsJson;
    concurrency::concurrent_queue<string> durationsJson;
    future<void> fConstructStatsJson = async(launch::async, [](map<string, int>& numberOfViews, unordered_map<string, float>& durations, float& averageDuration, concurrency::concurrent_queue<string>& viewsJson, concurrency::concurrent_queue<string>& durationsJson) {
        future<void> fConstructViewsJson = async(launch::async, [](map<string, int>& numberOfViews, concurrency::concurrent_queue<string>& viewsJson) {
            while (true) {
                if (viewsCalculated) {
                    int n = 0;
                    boolean firstValue = true;
                    viewsJson.push("  \"multiple_views\": [\n");
                    for (auto i = numberOfViews.begin(); i != numberOfViews.end(); ++i) {
                        if ((*i).second > 1) {
                            if (!firstValue) {
                                viewsJson.push(",\n");
                            }
                            firstValue = false;
                            string json = "    {\n      \"ip_address\": \"";
                            json += (*i).first;
                            json += "\",\n      \"views\": ";
                            json += to_string((*i).second);
                            json += "\n    }";
                            viewsJson.push(json);
                        }
                        ++n;
                    }
                    viewsJson.push("\n  ]");
                    stopOutputtingViewsToFile = true;
                    cout << "Constructed " << n << " views lines\n";
                    break;
                }
            }
        }, ref(numberOfViews), ref(viewsJson));
        future<void> fConstructDurationsJson = async(launch::async, [](unordered_map<string, float>& durations, float& averageDuration, concurrency::concurrent_queue<string>& durationsJson) {
            while (true) {
                if (durationsCalculated) {
                    int n = 0;
                    boolean firstValue = true;
                    durationsJson.push("  \"durations\": [\n");
                    for (auto i = durations.begin(); i != durations.end(); ++i) {
                        cout << (*i).first << "\n";
                        if (!firstValue) {
                            durationsJson.push(",");
                        }
                        firstValue = false;
                        string json = "    {\n      \"session_id\": \"";
                        json += (*i).first;
                        json += "\",\n      \"duration\": ";
                        json += to_string((*i).second);
                        json += "\n    }";
                        durationsJson.push(json);
                        ++n;
                    }
                    durationsJson.push("\n  ],\n");
                    if (averageDurationCalculated) {
                        string json = "  \"average_duration\": ";
                        json += to_string(averageDuration);
                        durationsJson.push(json);
                        ++n;
                    }
                    stopOutputtingDurationsToFile = true;
                    cout << "Constructed " << n << " durations lines\n";
                    break;
                }
            }
        }, ref(durations), ref(averageDuration), ref(durationsJson));
        fConstructViewsJson.get();
        fConstructDurationsJson.get();
    }, ref(numberOfViews), ref(durations), ref(averageDuration), ref(viewsJson), ref(durationsJson));
    future<void> fOutputLog = async(launch::async, [](concurrency::concurrent_queue<string>& logJson, const string& fileName) {
        ofstream jsonFile(fileName);
        int i = 0;
        while (true) {
            string json;
            auto gotValue = logJson.try_pop(json);
            if (gotValue) {
                outputToFile(move(json), ref(jsonFile));
                ++i;
            }
            else if (stopOutputtingToFile && logJson.empty()) {
                cout << "Output " << i << " log lines (should be +2)\n";
                break;
            }
        }
    }, ref(logJson), testFile + ".json");
    future<void> fOutputStats = async(launch::async, [](concurrency::concurrent_queue<string>& viewsJson, concurrency::concurrent_queue<string>& durationsJson, const string& fileName) {
        ofstream jsonFile(fileName);
        jsonFile << "{\n";
        string startWith = "";
        atomic<boolean> fileAvailable = true;
        future<void> fOutputViews = async(launch::async, [&startWith, &fileAvailable](concurrency::concurrent_queue<string>& viewsJson, ofstream& jsonFile) {
            bool firstValue = true;
            int i = 0;
            bool stopLooping = false;
            while (!stopLooping) {
                if (!viewsJson.empty() && fileAvailable) {
                    fileAvailable = false;
                    while (true) {
                        if (firstValue) {
                            outputToFile(move(startWith), jsonFile);
                            firstValue = false;
                            ++i;
                        }
                        else {
                            string json;
                            auto gotValue = viewsJson.try_pop(json);
                            if (gotValue) {
                                outputToFile(move(json), ref(jsonFile));
                                ++i;
                            }
                            else if (stopOutputtingViewsToFile && viewsJson.empty()) {
                                cout << "Output " << i << " view lines\n";
                                startWith = ",\n";
                                fileAvailable = true;
                                stopLooping = true;
                                break;
                            }
                        }
                    }
                }
            }
        }, ref(viewsJson), ref(jsonFile));
        future<void> fOutputDurations = async(launch::async, [&startWith, &fileAvailable](concurrency::concurrent_queue<string>& durationsJson, ofstream& jsonFile) {
            bool firstValue = true;
            int i = 0;
            bool stopLooping = false;
            while (!stopLooping) {
                if (!durationsJson.empty() && fileAvailable) {
                    fileAvailable = false;
                    while (true) {
                        if (firstValue) {
                            outputToFile(move(startWith), jsonFile);
                            firstValue = false;
                            ++i;
                        }
                        else {
                            string json;
                            auto gotValue = durationsJson.try_pop(json);
                            if (gotValue) {
                                outputToFile(move(json), ref(jsonFile));
                                ++i;
                            }
                            else if (stopOutputtingDurationsToFile && durationsJson.empty()) {
                                cout << "Output " << i << " duration lines\n";
                                startWith = ",\n";
                                fileAvailable = true;
                                stopLooping = true;
                                break;
                            }
                        }
                    }
                }
            }
        }, ref(durationsJson), ref(jsonFile));
        fOutputViews.get();
        fOutputDurations.get();
        jsonFile << "\n}";
    }, ref(viewsJson), ref(durationsJson), testFile + "_statistics.json");
    fParseLines.get();
    fConstructLogJson.get();
    fCalculateViews.get();
    fCalculateDurations.get();
    fConstructStatsJson.get();
    fOutputLog.get();
    fOutputStats.get();
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
        ifstream xmlFile(testFile + ".xml");
        string line;
        concurrency::concurrent_queue<string> q;
        future<void> f = async(launch::async, processLines, ref(q));
        // Parse XML file
        while (getline(xmlFile, line)) {
            q.push(move(line));
        }
        stopParsingLines = true;
        xmlFile.close();
        f.get();
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
