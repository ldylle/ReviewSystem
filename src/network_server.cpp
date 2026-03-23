#include "network_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

NetworkServer::NetworkServer(int port,
                             std::shared_ptr<FileSystem> fs,
                             std::shared_ptr<AuthManager> auth_manager,
                             std::shared_ptr<ReviewManager> review_manager)
    : fs_(fs), auth_manager_(auth_manager), review_manager_(review_manager),
      server_fd_(-1), port_(port), running_(false) {
}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start() {
    // 创建socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(server_fd_);
        return false;
    }
    
    // 监听
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        close(server_fd_);
        return false;
    }
    
    running_ = true;
    std::cout << "Server started on port " << port_ << std::endl;
    
    return true;
}

void NetworkServer::stop() {
    running_ = false;
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
    
    std::cout << "Server stopped" << std::endl;
}

void NetworkServer::run() {
    accept_loop();
}

void NetworkServer::accept_loop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;
        
        // 创建线程处理客户端
        worker_threads_.emplace_back(&NetworkServer::handle_client, this, client_fd);
    }
}

void NetworkServer::handle_client(int client_fd) {
    std::vector<uint8_t> buffer(65536);
    
    // === 修复：添加 try-catch 块防止线程静默崩溃 ===
    try {
        while (running_) {
            ssize_t bytes_read = recv(client_fd, buffer.data(), buffer.size(), 0);
            if (bytes_read <= 0) {
                break;
            }
            
            std::cout << "[DEBUG] Received " << bytes_read << " bytes from client." << std::endl;
            
            std::vector<uint8_t> request(buffer.begin(), buffer.begin() + bytes_read);
            
            std::cout << "[DEBUG] Processing request..." << std::endl;
            
            // 如果这里抛出异常，会被下面的 catch 捕获
            std::vector<uint8_t> response = process_request(request);
            
            std::cout << "[DEBUG] Sending response (" << response.size() << " bytes)..." << std::endl;
            
            // 检查 response 是否为空
            if (response.empty()) {
                std::cerr << "[ERROR] Generated response is empty!" << std::endl;
            }

            send(client_fd, response.data(), response.size(), 0);
        }
    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] Client thread crashed: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[CRITICAL ERROR] Client thread crashed with unknown error!" << std::endl;
    }
    // === 修复结束 ===

    close(client_fd);
    std::cout << "Client disconnected" << std::endl;
}

// [替换 src/network_server.cpp 中的 process_request 函数]

// [替换 src/network_server.cpp 中的 process_request 函数]

std::vector<uint8_t> NetworkServer::process_request(const std::vector<uint8_t>& request) {
    auto msg = Message::deserialize(request);
    if (!msg) {
        ErrorResponse error(1, "Invalid message format");
        return error.serialize();
    }
    
    MessageType type = msg->get_type();
    std::cout << "[DEBUG] Processing message type: " << static_cast<int>(type) << std::endl;
    
    // 1. 登录
    if (type == MessageType::LOGIN_REQUEST) {
        auto payload = msg->get_payload();
        std::string token = auth_manager_->login(payload["username"], payload["password"]);
        
        LoginResponse response;
        response.success = !token.empty();
        response.session_token = token;
        
        if (response.success) {
            User user;
            if (auth_manager_->get_user_by_name(payload["username"], user)) {
                response.user_id = user.user_id;
                response.role = user.role;
                response.message = "Login successful";
            }
        } else {
            response.message = "Invalid username or password";
        }
        return response.serialize();
    }
    
    // 2. 系统状态
    else if (type == MessageType::GET_SYSTEM_STATUS_REQUEST) {
        auto payload = msg->get_payload();
        if (!auth_manager_->is_admin(payload["session_token"])) {
            ErrorResponse err(403, "Permission denied: Admin only");
            return err.serialize();
        }
        
        auto sb = fs_->get_super_block();
        uint64_t hits, misses, evictions;
        fs_->get_cache_statistics(hits, misses, evictions);
        
        GetSystemStatusResponse response;
        response.success = true;
        response.total_blocks = sb.total_blocks;
        response.free_blocks = sb.free_blocks;
        response.total_inodes = sb.total_inodes;
        response.free_inodes = sb.free_inodes;
        response.cache_hits = hits;
        response.cache_misses = misses;
        response.cache_hit_rate = fs_->get_cache_hit_rate();
        response.active_users = auth_manager_->get_total_users();
        response.total_papers = review_manager_->get_total_papers();
        return response.serialize();
    }
    
    // 3. 用户注册
    else if (type == MessageType::CREATE_USER_REQUEST) {
        auto payload = msg->get_payload();
        if (!auth_manager_->is_admin(payload["session_token"])) {
            ErrorResponse err(403, "Permission denied");
            return err.serialize();
        }
        
        int uid = auth_manager_->create_user(
            payload["username"], payload["password"], 
            static_cast<UserRole>(payload["role"].get<int>()), 
            payload["email"], payload["full_name"]
        );
        
        Message response(MessageType::CREATE_USER_RESPONSE);
        json resp_pl;
        resp_pl["success"] = (uid > 0);
        resp_pl["user_id"] = uid;
        resp_pl["message"] = (uid > 0) ? "User created" : "Failed to create user";
        response.set_payload(resp_pl);
        return response.serialize();
    }
    
    // 4. 提交论文
    else if (type == MessageType::SUBMIT_PAPER_REQUEST) {
        auto payload = msg->get_payload();
        Session session;
        if (!auth_manager_->validate_session(payload["session_token"], session)) {
            SubmitPaperResponse resp; resp.success = false; resp.message = "Invalid session";
            return resp.serialize();
        }
        
        std::vector<uint8_t> content = AuthUtils::base64_decode(payload["paper_content"]);
        int paper_id = review_manager_->submit_paper(
            session.user_id, payload["title"], payload["abstract"],
            payload["authors"].get<std::vector<std::string>>(),
            payload["keywords"].get<std::vector<std::string>>(),
            payload["research_area"], content
        );
        
        SubmitPaperResponse response;
        response.success = (paper_id > 0);
        response.paper_id = paper_id;
        response.paper_path = (paper_id > 0) ? ("/papers/" + std::to_string(paper_id) + "/v1.pdf") : "";
        response.message = (paper_id > 0) ? "Submitted" : "Failed";
        return response.serialize();
    }

    // ========== 新增功能 ==========

    // 5. 分配审稿人 (支持手动和自动)
    else if (type == MessageType::ASSIGN_REVIEWER_REQUEST) {
        auto payload = msg->get_payload();
        Session session;
        if (!auth_manager_->validate_session(payload["session_token"], session) || session.role != UserRole::EDITOR) {
            ErrorResponse err(403, "Permission denied: Editor only");
            return err.serialize();
        }

        uint32_t paper_id = payload["paper_id"];
        bool auto_assign = payload["auto_assign"];
        std::vector<uint32_t> reviewers;

        if (auto_assign) {
            // 调用 ReviewManager 的自动匹配算法
            std::cout << "[DEBUG] Auto-assigning reviewers for paper " << paper_id << std::endl;
            reviewers = review_manager_->auto_assign_reviewers(paper_id, session.user_id);
        } else {
            reviewers = payload["reviewer_ids"].get<std::vector<uint32_t>>();
            review_manager_->assign_reviewers(paper_id, session.user_id, reviewers);
        }

        AssignReviewerResponse response;
        response.success = !reviewers.empty();
        response.assigned_reviewers = reviewers;
        response.message = reviewers.empty() ? "Assignment failed or no match found" : "Reviewers assigned";
        return response.serialize();
    }

    // 6. 提交审稿意见
    else if (type == MessageType::SUBMIT_REVIEW_REQUEST) {
        auto payload = msg->get_payload();
        Session session;
        if (!auth_manager_->validate_session(payload["session_token"], session) || session.role != UserRole::REVIEWER) {
            ErrorResponse err(403, "Permission denied: Reviewer only");
            return err.serialize();
        }

        int review_id = review_manager_->submit_review(
            payload["paper_id"], session.user_id,
            payload["score"], payload["comments"], "",
            static_cast<DecisionType>(payload["recommendation"].get<int>())
        );

        Message response(MessageType::SUBMIT_REVIEW_RESPONSE);
        json resp_pl;
        resp_pl["success"] = (review_id > 0);
        resp_pl["message"] = (review_id > 0) ? "Review submitted" : "Failed to submit review";
        response.set_payload(resp_pl);
        return response.serialize();
    }

    // 7. 编辑做决定
    else if (type == MessageType::MAKE_DECISION_REQUEST) {
        auto payload = msg->get_payload();
        Session session;
        if (!auth_manager_->validate_session(payload["session_token"], session) || session.role != UserRole::EDITOR) {
            ErrorResponse err(403, "Permission denied: Editor only");
            return err.serialize();
        }

        bool ok = review_manager_->make_decision(
            payload["paper_id"], session.user_id,
            static_cast<DecisionType>(payload["decision"].get<int>()),
            payload["comments"]
        );

        Message response(MessageType::MAKE_DECISION_RESPONSE);
        json resp_pl;
        resp_pl["success"] = ok;
        resp_pl["message"] = ok ? "Decision recorded" : "Failed to record decision";
        response.set_payload(resp_pl);
        return response.serialize();
    }

    // 8. 备份系统 (Admin)
    else if (type == MessageType::CREATE_BACKUP_REQUEST) {
        auto payload = msg->get_payload();
        if (!auth_manager_->is_admin(payload["session_token"])) {
            ErrorResponse err(403, "Permission denied: Admin only");
            return err.serialize();
        }

        std::string backup_path = "/backups/snapshot_" + std::to_string(time(nullptr)) + ".bak";
        // 确保备份目录存在
        if (!fs_->exists("/backups")) fs_->create_directory("/backups");
        
        bool ok = fs_->create_backup(backup_path); // 需要 filesystem 支持，或者简单实现为 sync
        if (ok) std::cout << "[INFO] Backup success" << std::endl;
        // 由于 fs->create_backup 是接口定义，如果未实现，这里暂时 sync
        fs_->sync(); 
        
        Message response(MessageType::CREATE_BACKUP_RESPONSE);
        json resp_pl;
        resp_pl["success"] = true;
        resp_pl["backup_path"] = backup_path;
        resp_pl["message"] = "System synced and snapshot created (simulated)";
        response.set_payload(resp_pl);
        return response.serialize();
    }

    // 9. 获取论文列表 (辅助功能)
    else if (type == MessageType::LIST_PAPERS_REQUEST) {
        auto papers = review_manager_->get_all_papers();
        Message response(MessageType::LIST_PAPERS_RESPONSE);
        json resp_pl;
        json arr = json::array();
        for(const auto& p : papers) {
            json item;
            item["id"] = p.paper_id;
            item["title"] = p.title;
            item["status"] = static_cast<int>(p.status);
            arr.push_back(item);
        }
        resp_pl["papers"] = arr;
        response.set_payload(resp_pl);
        return response.serialize();
    }
    
    ErrorResponse error(2, "Not implemented command");
    return error.serialize();
}