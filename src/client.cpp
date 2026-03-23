#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <iomanip>

Client::Client(const std::string& host, int port)
    : host_(host), port_(port), socket_fd_(-1), connected_(false) {
}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        close(socket_fd_);
        return false;
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to server" << std::endl;
        close(socket_fd_);
        return false;
    }
    
    connected_ = true;
    std::cout << "Connected to server at " << host_ << ":" << port_ << std::endl;
    return true;
}

void Client::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool Client::login(const std::string& username, const std::string& password) {
    LoginRequest request(username, password);
    auto request_data = request.serialize();
    
    auto response_data = send_request(request_data);
    auto msg = Message::deserialize(response_data);
    
    if (!msg || msg->get_type() != MessageType::LOGIN_RESPONSE) {
        std::cerr << "Login failed: Invalid response" << std::endl;
        return false;
    }
    
    auto payload = msg->get_payload();
    bool success = payload["success"];
    
    if (success) {
        session_token_ = payload["session_token"];
        std::cout << "Login successful!" << std::endl;
        
        uint32_t role_int = payload["role"];
        std::cout << "Role: " << ProtocolUtils::role_to_string(static_cast<UserRole>(role_int)) << std::endl;
        return true;
    } else {
        std::string msg_str = payload["message"];
        std::cout << "Login failed: " << msg_str << std::endl;
        return false;
    }
}

bool Client::logout() {
    session_token_.clear();
    std::cout << "Logged out" << std::endl;
    return true;
}

std::vector<uint8_t> Client::send_request(const std::vector<uint8_t>& request) {
    if (!connected_) return {};
    
    if (send(socket_fd_, request.data(), request.size(), 0) < 0) {
        std::cerr << "Send failed" << std::endl;
        disconnect();
        return {};
    }
    
    std::vector<uint8_t> response(1024 * 1024); // 1MB buffer
    ssize_t bytes_read = recv(socket_fd_, response.data(), response.size(), 0);
    
    if (bytes_read <= 0) {
        std::cerr << "Server disconnected" << std::endl;
        disconnect();
        return {};
    }
    
    response.resize(bytes_read);
    return response;
}

void Client::print_help() {
    std::cout << "\n=== Available Commands ===\n"
              << "  [Common]\n"
              << "    login <user> <pass>          : Login to system\n"
              << "    logout                       : Logout\n"
              << "    ls_papers                    : List all papers (IDs & Status)\n"
              << "    exit                         : Exit client\n"
              << "  [Author]\n"
              << "    submit <title> <file>        : Submit a new paper\n"
              << "  [Editor]\n"
              << "    assign <pid> <rid>...        : Assign reviewers manually\n"
              << "    auto_assign <pid>            : Auto-assign using algorithm\n"
              << "    decide <pid> <ACCEPT/REJECT> : Make final decision\n"
              << "  [Reviewer]\n"
              << "    review <pid> <score> <rec>   : Submit review (rec: 1=Acc, 2=Rej)\n"
              << "  [Admin]\n"
              << "    register <user> <pass> <role>: Register new user\n"
              << "    stat                         : View system statistics\n"
              << "    backup                       : Create system snapshot\n"
              << std::endl;
}

// [补上的函数] 之前漏掉了这个关键的主循环函数！
void Client::run_cli() {
    print_help();
    
    std::string line;
    while (connected_) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        
        if (line.empty()) continue;
        
        if (!process_command(line)) {
            break;
        }
    }
}

std::vector<uint8_t> read_local_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}

bool Client::process_command(const std::string& cmd_line) {
    std::istringstream iss(cmd_line);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "help") print_help();
    else if (cmd == "exit" || cmd == "quit") return false;
    
    else if (cmd == "login") {
        std::string u, p; iss >> u >> p;
        if (u.empty()) std::cout << "Usage: login <user> <pass>" << std::endl;
        else login(u, p);
    }
    else if (cmd == "logout") logout();
    
    // --- Admin Commands ---
    else if (cmd == "stat") {
        if (session_token_.empty()) { std::cout << "Login first." << std::endl; return true; }
        GetSystemStatusRequest req; req.session_token = session_token_;
        auto resp = Message::deserialize(send_request(req.serialize()));
        if(resp) {
            auto pl = resp->get_payload();
            if(pl["success"]) {
                std::cout << "Blocks: " << pl["free_blocks"] << "/" << pl["total_blocks"] 
                          << " | Inodes: " << pl["free_inodes"] 
                          << " | Users: " << pl["active_users"] 
                          << " | Papers: " << pl["total_papers"] << std::endl;
            } else std::cout << "Error: " << pl["message"] << std::endl;
        }
    }
    else if (cmd == "register") {
        std::string u, p, r; iss >> u >> p >> r;
        if (r.empty()) { std::cout << "Usage: register <user> <pass> <role>" << std::endl; return true; }
        Message msg(MessageType::CREATE_USER_REQUEST);
        json pl; pl["session_token"] = session_token_; pl["username"] = u; pl["password"] = p;
        pl["email"] = u + "@test.com"; pl["full_name"] = u;
        if(r=="ADMIN") pl["role"]=1; else if(r=="EDITOR") pl["role"]=2; 
        else if(r=="REVIEWER") pl["role"]=3; else pl["role"]=4;
        msg.set_payload(pl);
        auto resp = Message::deserialize(send_request(msg.serialize()));
        if(resp && resp->get_payload()["success"]) std::cout << "User ID: " << resp->get_payload()["user_id"] << std::endl;
        else std::cout << "Failed." << std::endl;
    }
    else if (cmd == "backup") {
        if (session_token_.empty()) return true;
        Message msg(MessageType::CREATE_BACKUP_REQUEST);
        json pl; pl["session_token"] = session_token_;
        msg.set_payload(pl);
        auto resp = Message::deserialize(send_request(msg.serialize()));
        if(resp) std::cout << resp->get_payload()["message"] << std::endl;
    }

    // --- Author Commands ---
    else if (cmd == "submit") {
        std::string t, f; iss >> t >> f;
        if (f.empty()) { std::cout << "Usage: submit <title> <file>" << std::endl; return true; }
        std::vector<uint8_t> c = read_local_file(f);
        if(c.empty()) { std::cout << "File error." << std::endl; return true; }
        SubmitPaperRequest req; req.session_token = session_token_; req.title = t; 
        req.paper_content = c; req.authors = {"Me"}; req.keywords = {"OS"}; req.abstract="Abs"; req.research_area="Sys";
        auto resp = Message::deserialize(send_request(req.serialize()));
        if(resp && resp->get_payload()["success"]) std::cout << "Submitted ID: " << resp->get_payload()["paper_id"] << std::endl;
        else std::cout << "Failed: " << resp->get_payload()["message"] << std::endl;
    }

    // --- Editor Commands ---
    else if (cmd == "assign") {
        uint32_t pid; iss >> pid;
        std::vector<uint32_t> rids; uint32_t r;
        while(iss >> r) rids.push_back(r);
        if(rids.empty()) { std::cout << "Usage: assign <pid> <rid1> [rid2...]" << std::endl; return true; }
        
        AssignReviewerRequest req; req.session_token = session_token_; req.paper_id = pid; 
        req.reviewer_ids = rids; req.auto_assign = false;
        auto resp = Message::deserialize(send_request(req.serialize()));
        if(resp) std::cout << resp->get_payload()["message"] << std::endl;
    }
    else if (cmd == "auto_assign") {
        uint32_t pid; iss >> pid;
        AssignReviewerRequest req; req.session_token = session_token_; req.paper_id = pid; req.auto_assign = true;
        auto resp = Message::deserialize(send_request(req.serialize()));
        if(resp) {
            auto pl = resp->get_payload();
            if(pl["success"]) {
                std::cout << "Auto-assigned reviewers: ";
                for(auto id : pl["assigned_reviewers"]) std::cout << id << " ";
                std::cout << std::endl;
            } else std::cout << "Failed: " << pl["message"] << std::endl;
        }
    }
    else if (cmd == "decide") {
        uint32_t pid; std::string dec_str; iss >> pid >> dec_str;
        Message msg(MessageType::MAKE_DECISION_REQUEST);
        json pl; pl["session_token"] = session_token_; pl["paper_id"] = pid; pl["comments"] = "Final decision";
        if(dec_str == "ACCEPT") pl["decision"] = 1; else pl["decision"] = 2; // 1=Accept, 2=Reject
        msg.set_payload(pl);
        auto resp = Message::deserialize(send_request(msg.serialize()));
        if(resp) std::cout << resp->get_payload()["message"] << std::endl;
    }

    // --- Reviewer Commands ---
    else if (cmd == "review") {
        uint32_t pid; int score, rec; iss >> pid >> score >> rec;
        if(rec == 0) { std::cout << "Usage: review <pid> <score> <rec(1=Acc,2=Rej)>" << std::endl; return true; }
        Message msg(MessageType::SUBMIT_REVIEW_REQUEST);
        json pl; pl["session_token"] = session_token_; pl["paper_id"] = pid; 
        pl["score"] = score; pl["recommendation"] = rec; pl["comments"] = "Good paper";
        msg.set_payload(pl);
        auto resp = Message::deserialize(send_request(msg.serialize()));
        if(resp) std::cout << resp->get_payload()["message"] << std::endl;
    }
    
    // --- Utility ---
    else if (cmd == "ls_papers") {
        if (session_token_.empty()) return true;
        Message msg(MessageType::LIST_PAPERS_REQUEST);
        json pl; pl["session_token"] = session_token_; msg.set_payload(pl);
        auto resp = Message::deserialize(send_request(msg.serialize()));
        if(resp) {
            auto arr = resp->get_payload()["papers"];
            std::cout << "--- Paper List ---" << std::endl;
            for(auto& p : arr) {
                std::cout << "ID: " << p["id"] << " | Title: " << p["title"] << " | Status: " << p["status"] << std::endl;
            }
        }
    }
    else {
        std::cout << "Unknown command." << std::endl;
    }
    return true;
}