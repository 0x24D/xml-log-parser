# xml-log-parser

* The aim of this project is to demonstrate how to maximise throughput using the multithreading capabilities of C++.

## Background
> The data comprises excerpts from a log file for a web site belonging to a large eCommerce retailer, who has many thousands of page impressions per day.
> The data in the log file contains details of which specific pages customers looked at, the time they looked at these, the customer’s IP addresses and the web browser they used. Each customer generates a trace for a given ‘session’, which is identified by a ‘session id’. This ‘trace’ includes all the pages a user browsed during that session. For each page, the date and time of access is given.

## Purpose
* Translate the XML file to a corresponding JSON formatted file. This should have the '.json' file extension. (implemented - single threaded)
* Generate a second JSON formatted file that contains statistics for the usage of the website:
  * The time duration for each session (i.e. the time duration for each session, from the first page access to the last). (implemented, no file output - single threaded)
  * The average time users spend on the site. (implemented, no file output - single threaded)
  * The number of times users with the same IP address visit the site.

```
// log.json
{
  "entry":  [
    {
      "session_id": $session_id,
      "ip_address": $ip_address,
      "browser": $browser,
      "page_views": [
        {
          "path": $path,
          "time": $time
        },
        {
          "path": $path2,
          "time": $time2
        }
      ]
    ]
  }
}

// C++
std::vector<
session_id: string,
ip_address: string
browser: string
paths: std::vector<string>
times: std::vector<string>>
... because maps are bad for performance

// statistics.json
{
  "statistics": {
    "time_durations": [
      {
        "session_id": $session_id,
        "duration": $duration
      }
    ],
    "average_time": $average_time,
    // either
    "duplicate_views": $duplicate_views
    // or
    "duplicate_views": [
      {
        "ip_address": $ip_address,
        "views": $views
      }
    ]
  }
}
```
