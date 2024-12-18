#pragma once
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
#include "Proxy.h"

#pragma comment(lib, "ws2_32.lib")

void HttpProxy::load_blocked_sites(const std::string& filename) {
    // ADD EDIT TO GUI
    std::ifstream file(filename);
    std::string line;
    blocked_sites_trie = new DomainTrie();

    while (std::getline(file, line)) {
        // Remove whitespace and convert to lowercase
        line.erase(remove_if(line.begin(), line.end(), isspace),
            line.end());
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);

        if (!line.empty() &&
            line[0] != '#') {  // Skip empty lines and comments
            blocked_sites_trie->insert(line);
        }
    }
    std::cout << "Loaded blocked sites into Trie" << std::endl;
}

bool HttpProxy::is_site_blocked(const std::string& host) {
    std::string lowercase_host = host;
    std::transform(lowercase_host.begin(), lowercase_host.end(),
        lowercase_host.begin(), ::tolower);
    return blocked_sites_trie->search(lowercase_host);
}

void HttpProxy::send_blocked_response(SOCKET client_socket, const std::string& host) {
    std::string blocked_page =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html><head><title>Access Denied</title></head>"
        "<body style='text-align: center; font-family: Arial, sans-serif;'>"
        "<h1>Access Denied</h1>"
        "<p>Access to " +
        host +
        " has been blocked by the proxy administrator.</p>"
        "</body></html>";

    send(client_socket, blocked_page.c_str(), blocked_page.length(), 0);
}

//void HttpProxy::log_request(const HttpRequest& req, const std::string& client_ip,
//    const bool blocked) {
//    this->log_queue->add_log(req, client_ip, blocked);
//
//    std::cout << "\n"
//        << get_timestamp() << " Request Details:" << std::endl;
//    std::cout << "|- Method: " << req.method << std::endl;
//    std::cout << "|- URL: " << req.full_url << std::endl;
//    std::cout << "|- Host: " << req.host << std::endl;
//    std::cout << "|- Port: " << req.port << std::endl;
//    std::cout << "|- Path: " << req.path << std::endl;
//    std::cout << "\\- Client IP: " << client_ip << std::endl;
//}

HttpRequest HttpProxy::parse_request(const std::string& request) {
    HttpRequest req;
    std::istringstream iss(request);
    std::string line;

    std::getline(iss, line);
    std::istringstream first_line(line);
    first_line >> req.method;
    std::string url;
    first_line >> url;

    req.full_url = url;

    size_t host_start = url.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
    }
    else {
        host_start = 0;
    }

    size_t path_start = url.find("/", host_start);
    if (path_start == std::string::npos) {
        path_start = url.length();
        req.path = "/";
    }
    else {
        req.path = url.substr(path_start);
    }

    std::string host_port = url.substr(host_start, path_start - host_start);
    size_t port_pos = host_port.find(":");
    if (port_pos != std::string::npos) {
        req.host = host_port.substr(0, port_pos);
        req.port = std::stoi(host_port.substr(port_pos + 1));
    }
    else {
        req.host = host_port;
        req.port = 80;
    }

    if (req.method == "CONNECT") {
        size_t colon_pos = url.find(":");
        if (colon_pos != std::string::npos) {
            req.host = url.substr(0, colon_pos);
            req.port = std::stoi(url.substr(colon_pos + 1));
            req.full_url = "https://" + url;
        }
    }

    while (std::getline(iss, line) && line != "\r" && line != "") {
        if (line.substr(0, 6) == "Host: ") {
            std::string host_header = line.substr(6);
            size_t header_port_pos = host_header.find(":");
            if (header_port_pos != std::string::npos) {
                req.host = host_header.substr(0, header_port_pos);
                req.port =
                    std::stoi(host_header.substr(header_port_pos + 1));
            }
        }
        req.headers += line + "\r\n";
    }

    return req;
}

void HttpProxy::handle_client(SOCKET client_socket, const std::string& client_ip) {
    char buffer[BUFFER_SIZE];
    ZeroMemory(buffer, BUFFER_SIZE);

    // from client to proxy (1)
    int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string request(buffer);
    HttpRequest req = parse_request(request);

    bool blocked = is_site_blocked(req.host);

    if (blocked) {
        this->log_queue->add_log(req, client_ip, true, request);
        if (req.method != "CONNECT")
            send_blocked_response(client_socket, req.host);
        closesocket(client_socket);
        return;
    }
    if (req.method == "CONNECT") {
        this->log_queue->add_log(req, client_ip, false, request);
        handle_https_tunnel(client_socket, req);
    }
    else {
        handle_http_request(client_socket, req, request, client_ip);
    }
}

void HttpProxy::handle_https_tunnel(SOCKET client_socket, const HttpRequest& req) {
    SOCKET remote_socket = connect_to_host(req.host, req.port);
    if (remote_socket == INVALID_SOCKET) {
        send_error(client_socket, "Failed to connect to remote server");
        closesocket(client_socket);
        return;
    }

    const char* response = "HTTP/1.1 200 Connection established\r\n\r\n";
    send(client_socket, response, strlen(response), 0);

    tunnel_traffic(client_socket, remote_socket);
}

void HttpProxy::handle_http_request(SOCKET client_socket, const HttpRequest& req,
    const std::string& original_request, const std::string client_ip) {
    SOCKET remote_socket = connect_to_host(req.host, req.port);
    if (remote_socket == INVALID_SOCKET) {
        send_error(client_socket, "Failed to connect to remote server");
        closesocket(client_socket);
        return;
    }

    // send from proxy to real server (2)
    send(remote_socket, original_request.c_str(), original_request.length(),
        0);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    // receive from real server to proxy (3)
    std::string response = "";
    while ((bytes_read = recv(remote_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        response += std::string(buffer);
        // send from proxy to client (4)
        send(client_socket, buffer, bytes_read, 0);
    }
    // std::cout << "-----------------------------------------" << std::endl;
    // std::cout << "From thread " << std::this_thread::get_id() << std::endl;
    // std::cout << response << std::endl;
    // std::cout << "-----------------------------------------" << std::endl;
    this->log_queue->add_log(req, client_ip, false, original_request,
        response);
    closesocket(remote_socket);
    closesocket(client_socket);
}

SOCKET HttpProxy::connect_to_host(const std::string& host, int port) {
    struct addrinfo hints, * res;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // resolve IP?
    int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(),
        &hints, &res);
    if (status != 0) {
        std::cerr << "Failed to resolve hostname: " << host << std::endl;
        return INVALID_SOCKET;
    }

    SOCKET sock =
        socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR) {
        closesocket(sock);
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return sock;
}

void HttpProxy::tunnel_traffic(SOCKET client_socket, SOCKET remote_socket) {
    fd_set read_fds;
    char buffer[BUFFER_SIZE];

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(remote_socket, &read_fds);

        // Note: Windows select() first parameter is ignored, but we'll keep
        // it for consistency
        int max_fd = 1;
        if (select(max_fd, &read_fds, nullptr, nullptr, nullptr) ==
            SOCKET_ERROR) {
            break;
        }

        if (FD_ISSET(client_socket, &read_fds)) {
            int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0 ||
                send(remote_socket, buffer, bytes_read, 0) <= 0) {
                break;
            }
        }

        if (FD_ISSET(remote_socket, &read_fds)) {
            int bytes_read = recv(remote_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0 ||
                send(client_socket, buffer, bytes_read, 0) <= 0) {
                break;
            }
        }
    }

    closesocket(client_socket);
    closesocket(remote_socket);
}

void HttpProxy::send_error(SOCKET client_socket, const std::string& message) {
    std::string response =
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " +
        std::to_string(message.length()) +
        "\r\n"
        "\r\n" +
        message;
    send(client_socket, response.c_str(), response.length(), 0);
}

HttpProxy::HttpProxy(int port, const std::string& blocklist_file, LogQueue* log_queue) : running(true) {
    this->log_queue = log_queue;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }

    load_blocked_sites(blocklist_file);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        WSACleanup();
        throw std::runtime_error("Failed to create server socket");
    }

    BOOL opt = TRUE;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt,
        sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr,
        sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(server_socket);
        WSACleanup();
        throw std::runtime_error("Failed to bind server socket");
        std::cerr << "Failed to bind server socket" << std::endl;
    }

    if (listen(server_socket, MAX_CONNECTIONS) == SOCKET_ERROR) {
        closesocket(server_socket);
        WSACleanup();
        throw std::runtime_error("Failed to listen on server socket");
        std::cerr << "Failed to listen on server socket" << std::endl;
    }
}

void HttpProxy::start() {
    std::cout << get_timestamp()
        << " HTTP Proxy Server started. Configure your browser to "
        "use this proxy."
        << std::endl;

    while (running) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        SOCKET client_socket =
            accept(server_socket, (struct sockaddr*)&client_addr,
                &client_addr_len);

        if (client_socket == INVALID_SOCKET) {
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip,
            INET_ADDRSTRLEN);

        client_threads.emplace_back(&HttpProxy::handle_client, this,
            client_socket, std::string(client_ip));
    }

    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void HttpProxy::stop() {
    running = false;
    closesocket(server_socket);
    WSACleanup();
}

