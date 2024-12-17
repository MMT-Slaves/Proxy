#pragma once
#ifndef PROXY_H
#define PROXY_H
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

#include "DomainTrie.h"
#include "LogQueue.h"

#pragma comment(lib, "ws2_32.lib")

class HttpProxy {
private:
    LogQueue* log_queue;
    SOCKET server_socket;
    std::vector<std::thread> client_threads;
    bool running;
    static const int BUFFER_SIZE = 8192;
    static const int MAX_CONNECTIONS = 100;
    DomainTrie blocked_sites_trie;

    void load_blocked_sites(const std::string& filename);

    bool is_site_blocked(const std::string& host);

    void send_blocked_response(SOCKET client_socket, const std::string& host);

    void log_request(const HttpRequest& req, const std::string& client_ip, const bool blocked);

    HttpRequest parse_request(const std::string& request);

    void handle_client(SOCKET client_socket, const std::string& client_ip);

    void handle_https_tunnel(SOCKET client_socket, const HttpRequest& req);

    void handle_http_request(SOCKET client_socket, const HttpRequest& req, const std::string& original_request, const std::string client_ip);

    SOCKET connect_to_host(const std::string& host, int port);

    void tunnel_traffic(SOCKET client_socket, SOCKET remote_socket);

    void send_error(SOCKET client_socket, const std::string& message);

public:
    HttpProxy(int port, const std::string& blocklist_file, LogQueue* log_queue) ;

    void start();

    void stop();

    ~HttpProxy() { stop(); }
};

#endif
