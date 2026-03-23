#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include "filesystem.h"
#include "auth_manager.h"
#include "review_manager.h"
#include "protocol.h"
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

class NetworkServer {
private:
    std::shared_ptr<FileSystem> fs_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<ReviewManager> review_manager_;
    
    int server_fd_;
    int port_;
    std::atomic<bool> running_;
    std::vector<std::thread> worker_threads_;
    
public:
    NetworkServer(int port, 
                 std::shared_ptr<FileSystem> fs,
                 std::shared_ptr<AuthManager> auth_manager,
                 std::shared_ptr<ReviewManager> review_manager);
    ~NetworkServer();
    
    bool start();
    void stop();
    void run();
    
private:
    void accept_loop();
    void handle_client(int client_fd);
    std::vector<uint8_t> process_request(const std::vector<uint8_t>& request);
};

#endif // NETWORK_SERVER_H
