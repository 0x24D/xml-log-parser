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

void outputToFile(const string& line) {
    cout << line << "\n";
}

string constructJsonLine(LogItem& item) {
    return item.sessionId;
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

void processLines(concurrency::concurrent_queue<string>& q) {
    concurrency::concurrent_queue<LogItem> q2;
    future<void> f = async(launch::async, [](concurrency::concurrent_queue<string>& q, concurrency::concurrent_queue<LogItem>& q2) {
        while (true) {
            string line;
            auto gotValue = q.try_pop(line);
            if (gotValue) {
                LogItem item = parseLogLine(move(line));
                q2.push(move(item));
            }
            else if (stopParsingLines) {
                break;
            }
        }
    }, ref(q), ref(q2));
    concurrency::concurrent_queue<string> q3;
    future<void> f2 = async(launch::async, [](concurrency::concurrent_queue<LogItem>& q2, concurrency::concurrent_queue<string>& q3) {
    while (true) {
        LogItem item;
        auto gotValue = q2.try_pop(item);
        if (gotValue) {
            string json = constructJsonLine(item);
            q3.push(move(json));
        }
        else if (stopParsingLines) {
            break;
        }
    }
    }, ref(q2), ref(q3));
    future<void> f3 = async(launch::async, [](concurrency::concurrent_queue<string>& q3) {
    while (true) {
        string json;
        auto gotValue = q3.try_pop(json);
        if (gotValue) {
            outputToFile(move(json));
        }
        else if (stopParsingLines) {
            break;
        }
    }
    }, ref(q3));
    f.get();
    f2.get();
    f3.get();
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

        const string testFile = "testdata\\2";
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
