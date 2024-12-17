#pragma once
#ifndef LOGQUEUE_H
#define LOGQUEUE_H
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

#define MAX_LOG_SIZE 1000

std::string get_timestamp();

struct HttpRequest {
    std::string method;
    std::string host;
    int port;
    std::string path;
    std::string headers;
    std::string full_url;
};

struct LogEntry {
    uint64_t id;               // Unique identifier
    std::string method;        // HTTP Method
    std::string host;          // Host
    int port;                  // Port
    std::string path;          // Path
    std::string client_ip;     // Client IP
    bool blocked;              // Blocked status
    std::string timestamp;     // Timestamp
    std::string fullRequest;   // Full request details
    std::string fullResponse;  // Full response details
};

class LogQueue {
private:
    std::mutex mutex;
    std::vector<LogEntry> logs;
    uint64_t next_id = 1;

public:
    LogQueue() {

    }

    void add_log(const HttpRequest& req, const std::string& client_ip,
        bool blocked, const std::string& full_request = "",
        const std::string& full_response = "");

    std::vector<LogEntry> get_logs();
};


#endif
