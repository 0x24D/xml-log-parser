// Performance2.cpp : Defines the entry point for the console application.
//

#include "Performance2.h"
#include "stdafx.h"
#include <concurrent_queue.h>
#include <ppl.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Timer - used to established precise timings for code.
class TIMER {
  LARGE_INTEGER t_;

  __int64 current_time_;

 public:
  TIMER()  // Default constructor. Initialises this timer with the current value
           // of the hi-res CPU timer.
  {
    QueryPerformanceCounter(&t_);
    current_time_ = t_.QuadPart;
  }

  TIMER(const TIMER &ct)  // Copy constructor.
  {
    current_time_ = ct.current_time_;
  }

  TIMER &operator=(const TIMER &ct)  // Copy assignment.
  {
    current_time_ = ct.current_time_;
    return *this;
  }

  TIMER &operator=(const __int64 &n)  // Overloaded copy assignment.
  {
    current_time_ = n;
    return *this;
  }

  ~TIMER() {}  // Destructor.

  static __int64 get_frequency() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
  }

  __int64 get_time() const { return current_time_; }

  void get_current_time() {
    QueryPerformanceCounter(&t_);
    current_time_ = t_.QuadPart;
  }

  inline bool operator==(const TIMER &ct) const {
    return current_time_ == ct.current_time_;
  }

  inline bool operator!=(const TIMER &ct) const {
    return current_time_ != ct.current_time_;
  }

  __int64 operator-(const TIMER &ct)
      const  // Subtract a TIMER from this one - return the result.
  {
    return current_time_ - ct.current_time_;
  }

  inline bool operator>(const TIMER &ct) const {
    return current_time_ > ct.current_time_;
  }

  inline bool operator<(const TIMER &ct) const {
    return current_time_ < ct.current_time_;
  }

  inline bool operator<=(const TIMER &ct) const {
    return current_time_ <= ct.current_time_;
  }

  inline bool operator>=(const TIMER &ct) const {
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

atomic<bool> stopConstructingDurations(false);
atomic<bool> stopConstructingJson(false);
atomic<bool> stopCalculatingViews(false);
atomic<bool> stopCalculatingDurations(false);
atomic<bool> stopOutputtingDurationsToFile(false);
atomic<bool> stopOutputtingToFile(false);
atomic<bool> stopOutputtingViewsToFile(false);
atomic<bool> stopParsingLines(false);
atomic<bool> viewsCalculated(false);

auto calculateDurationsAndAverage(
    concurrency::concurrent_queue<LogItem> &logData,
    concurrency::concurrent_queue<pair<string, int>> &durations,
    float &averageDuration) {
  auto i = 0;
  auto total = 0;
  while (true) {
    LogItem item;
    const auto gotValue = logData.try_pop(item);
    if (gotValue) {
      int duration;
      if (item.pathTimes.size() == 1) {
        duration = 0;
      } else {
        struct tm latestDateTm {};
        istringstream(item.pathTimes[item.pathTimes.size() - 1].second) >>
            std::get_time(&latestDateTm, "%d/%m/%Y %H:%M:%S");

        struct tm oldestDateTm {};
        istringstream(item.pathTimes[0].second) >>
            std::get_time(&oldestDateTm, "%d/%m/%Y %H:%M:%S");

        duration = static_cast<int>(
            round(difftime(mktime(&latestDateTm), mktime(&oldestDateTm))));
      }
      durations.push({item.sessionId, duration});
      total += duration;
      ++i;
    } else if (stopCalculatingDurations && logData.empty()) {
      stopConstructingDurations = true;
      averageDuration = static_cast<float>(total) / static_cast<float>(i);
      break;
    }
  }
}

auto calculateNumberOfViews(concurrency::concurrent_queue<string> &ipAddresses,
                            unordered_map<string, int> &numberOfViews) {
  while (true) {
    string ipAddress;
    const auto gotValue = ipAddresses.try_pop(ipAddress);
    if (gotValue) {
      auto it = numberOfViews.find(ipAddress);
      if (it == numberOfViews.end()) {
        numberOfViews.insert({ipAddress, 1});
      } else {
        ++it->second;
      }
    } else if (stopCalculatingViews && ipAddresses.empty()) {
      viewsCalculated = true;
      break;
    }
  }
}

auto constructDurationsJson(
    concurrency::concurrent_queue<pair<string, int>> &durations,
    float &averageDuration,
    concurrency::concurrent_queue<string> &durationsJson) {
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
      string json("    {\n      \"session_id\": \"");
      json += p.first;
      json += "\",\n      \"duration\": ";
      json += to_string(p.second);
      json += "\n    }";
      durationsJson.push(json);
    } else if (stopConstructingDurations && durations.empty()) {
      string json("\n  ],\n");
      json += "  \"average_duration\": ";
      json += to_string(averageDuration);
      durationsJson.push(json);
      stopOutputtingDurationsToFile = true;
      break;
    }
  }
}

auto constructLogJson(concurrency::concurrent_queue<LogItem> &logData,
                      concurrency::concurrent_queue<string> &logJson) {
  auto firstValue = true;
  logJson.push("{\n  \"entries\": [\n");
  while (true) {
    LogItem item;
    const auto gotValue = logData.try_pop(item);
    if (gotValue) {
      if (!firstValue) {
        logJson.push(",\n");
      } else {
        firstValue = false;
      }
      string json("    {\n");
      json += R"(      "session_id": ")" + item.sessionId + "\",\n";
      json += R"(      "ip_address": ")" + item.ipAddress + "\",\n";
      json += R"(      "browser": ")" + item.browser + "\",\n";
      json += "      \"page_views\": [\n";
      for (auto i = item.pathTimes.begin(); i != item.pathTimes.end(); ++i) {
        json += "        {\n";
        json += R"(          "path": ")" + (*i).first + "\",\n";
        json += R"(          "time": ")" + (*i).second + "\"\n";
        json += "        }";
        if (i != item.pathTimes.end() - 1) {
          json += ",";
        }
        json += "\n";
      }
      json += "      ]\n    }";
      logJson.push(move(json));
    } else if (stopConstructingJson && logData.empty()) {
      stopOutputtingToFile = true;
      break;
    }
  }
  logJson.push("\n  ]\n}");
}

auto constructViewsJson(unordered_map<string, int> &numberOfViews,
                        concurrency::concurrent_queue<string> &viewsJson) {
  while (true) {
    if (viewsCalculated) {
      boolean firstValue = true;
      viewsJson.push("  \"multiple_views\": [\n");
      for (const auto &v : numberOfViews) {
        if (v.second > 1) {
          if (!firstValue) {
            viewsJson.push(",\n");
          }
          firstValue = false;
          string json("    {\n      \"ip_address\": \"");
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
}

// Required by functions below
auto outputToFile(const string &line, ofstream &file) { file << line; }

auto outputLog(concurrency::concurrent_queue<string> &logJson,
               const string &fileName) {
  ofstream jsonFile(fileName);
  while (true) {
    string json;
    const auto gotValue = logJson.try_pop(json);
    if (gotValue) {
      outputToFile(json, ref(jsonFile));
    } else if (stopOutputtingToFile && logJson.empty()) {
      break;
    }
  }
}

auto outputStatistics(concurrency::concurrent_queue<string> &viewsJson,
                      concurrency::concurrent_queue<string> &durationsJson,
                      const string &fileName) {
  ofstream jsonFile(fileName);
  jsonFile << "{\n";
  string startWith;
  auto firstValue = true;
  while (true) {
    if (!durationsJson.empty()) {
      if (firstValue) {
        outputToFile(startWith, jsonFile);
        firstValue = false;
      } else {
        string json;
        const auto gotValue = durationsJson.try_pop(json);
        if (gotValue) {
          outputToFile(json, jsonFile);
        }
      }
    } else if (stopOutputtingDurationsToFile) {
      startWith = ",\n";
      break;
    }
  }
  firstValue = true;
  while (true) {
    if (!viewsJson.empty()) {
      if (firstValue) {
        outputToFile(startWith, jsonFile);
        firstValue = false;
      } else {
        string json;
        const auto gotValue = viewsJson.try_pop(json);
        if (gotValue) {
          outputToFile(json, jsonFile);
        }
      }
    } else if (stopOutputtingViewsToFile) {
      startWith = ",\n";
      break;
    }
  }
  jsonFile << "\n}";
}

// Required by function below
auto parseLogLine(const string &line) {
  const string sessionStartTag("<sessionid>");
  const string sessionEndTag("</sessionid>");
  const string ipStartTag("<ipaddress>");
  const string ipEndTag("</ipaddress>");
  const string browserStartTag("<browser>");
  const string browserEndTag("</browser>");
  const string pathStartTag("<path>");
  const string pathEndTag("</path>");
  const string timeStartTag("<time>");
  const string timeEndTag("</time>");
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

auto parseLines(concurrency::concurrent_queue<string> &unProcessedLines,
                concurrency::concurrent_queue<LogItem> &logData,
                concurrency::concurrent_queue<LogItem> &logDataCopy,
                concurrency::concurrent_queue<string> &ipAddresses) {
  while (true) {
    string line;
    const auto gotValue = unProcessedLines.try_pop(line);
    if (gotValue) {
      auto item = parseLogLine(line);
      ipAddresses.push(ref(item.ipAddress));
      logData.push(ref(item));
      logDataCopy.push(move(item));
    } else if (stopParsingLines && unProcessedLines.empty()) {
      stopConstructingJson = true;
      stopCalculatingViews = true;
      stopCalculatingDurations = true;
      break;
    }
  }
}

auto processLines(concurrency::concurrent_queue<string> &unProcessedLines) {
  concurrency::concurrent_queue<LogItem> logData;
  concurrency::concurrent_queue<LogItem> logDataCopy;
  concurrency::concurrent_queue<string> ipAddresses;
  thread tParseLines(parseLines, ref(unProcessedLines), ref(logData),
                     ref(logDataCopy), ref(ipAddresses));

  concurrency::concurrent_queue<string> logJson;
  thread tConstructLogJson(constructLogJson, ref(logData), ref(logJson));

  concurrency::concurrent_queue<pair<string, int>> durations;
  float averageDuration;
  thread tCalculateDurations(calculateDurationsAndAverage, ref(logDataCopy),
                             ref(durations), ref(averageDuration));

  // A unordered mao is optimized for searching (better than ordered map) - A
  // Tour of C++, sections 9.5-9.6
  unordered_map<string, int> numberOfViews;
  thread tCalculateViews(calculateNumberOfViews, ref(ipAddresses),
                         ref(numberOfViews));

  concurrency::concurrent_queue<string> viewsJson;
  concurrency::concurrent_queue<string> durationsJson;
  thread tConstructStatsJson(
      [](unordered_map<string, int> &numberOfViews,
         concurrency::concurrent_queue<pair<string, int>> &durations,
         float &averageDuration,
         concurrency::concurrent_queue<string> &viewsJson,
         concurrency::concurrent_queue<string> &durationsJson) {
        thread tConstructViewsJson(constructViewsJson, ref(numberOfViews),
                                   ref(viewsJson));
        thread tConstructDurationsJson(constructDurationsJson, ref(durations),
                                       ref(averageDuration),
                                       ref(durationsJson));
        tConstructDurationsJson.join();
        tConstructViewsJson.join();
      },
      ref(numberOfViews), ref(durations), ref(averageDuration), ref(viewsJson),
      ref(durationsJson));

  thread tOutputLog(outputLog, ref(logJson), "log.json");
  thread tOutputStats(outputStatistics, ref(viewsJson), ref(durationsJson),
                      "statistics.json");

  tParseLines.join();
  tConstructLogJson.join();
  tCalculateDurations.join();
  tCalculateViews.join();
  tConstructStatsJson.join();
  tOutputLog.join();
  tOutputStats.join();
}

int _tmain(int argc, TCHAR *argv[], TCHAR *envp[]) {
  int nRetCode = 0;

  // initialize Microsoft Foundation Classes, and print an error if failure
  if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) {
    _tprintf(_T("Fatal Error: MFC initialization failed\n"));
    nRetCode = 1;
  } else {
    // Application starts here...

    // Time the application's execution time.
    TIMER start;  // DO NOT CHANGE THIS LINE. Timing will start here.

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

    double elapsed_seconds =
        (double)elapsed.get_time() / (double)ticks_per_second;

    cout << "Elapsed time (seconds): " << elapsed_seconds;
    cout << endl;
    cout << "Press a key to continue" << endl;

    char c;
    cin >> c;
  }

  return nRetCode;
}
