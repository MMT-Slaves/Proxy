#include "LogQueue.h"
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


std::string get_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S]");
    return oss.str();
}

struct HttpRequest;

struct LogEntry;

void LogQueue::add_log(const HttpRequest& req, const std::string& client_ip,
    bool blocked, const std::string& full_request,
    const std::string& full_response) {
    std::lock_guard<std::mutex> lock(mutex);

    LogEntry entry{
        next_id++,        // Unique ID
        req.method,       // Method
        req.host,         // Host
        req.port,         // Port
        req.path,         // Path
        client_ip,        // Client IP
        blocked,          // Blocked status
        get_timestamp(),  // Timestamp
        full_request,     // Full request details
        full_response     // Full response details
    };

    logs.push_back(entry);

    // Optional: Limit log size
    if (logs.size() > MAX_LOG_SIZE) {
        logs.erase(logs.begin());
    }
}

std::vector<LogEntry> LogQueue::get_logs() {
    std::lock_guard<std::mutex> lock(mutex);
    return logs;
}

