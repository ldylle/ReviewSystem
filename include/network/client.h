#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include <string>
#include <vector>

class Client {
private:
    std::string host_;
    int port_;
    int socket_fd_;
    std::string session_token_;
    bool connected_;
    
public:
    Client(const std::string& host, int port);
    ~Client();
    
    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }
    
    // 认证
    bool login(const std::string& username, const std::string& password);
    bool logout();
    
    // 发送请求
    std::vector<uint8_t> send_request(const std::vector<uint8_t>& request);
    
    // 交互式CLI
    void run_cli();
    
private:
    void print_help();
    bool process_command(const std::string& cmd_line);
};

#endif // CLIENT_H
