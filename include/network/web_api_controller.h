#ifndef WEB_API_CONTROLLER_H
#define WEB_API_CONTROLLER_H

#include "web_server.h"
#include "filesystem.h"
#include "auth_manager.h"
#include "review_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <algorithm> // for std::find
 #include <filesystem>

// ==================== Web API 控制器 ====================
// 提供 RESTful API 接口，连接前端和后端业务逻辑
class WebApiController : public ApiController {
private:
    std::shared_ptr<FileSystem> fs_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<ReviewManager> review_manager_;

    // [Helper] 安全获取ID，兼容数字和字符串格式
    uint32_t safe_get_id(const json& j, const std::string& key) {
        if (!j.contains(key)) return 0;
        const auto& val = j[key];
        if (val.is_number()) return val.get<uint32_t>();
        if (val.is_string()) {
            try { return std::stoul(val.get<std::string>()); } catch (...) { return 0; }
        }
        return 0;
    }

    // [Helper] 检查用户是否有权限访问该论文
    // Author: 只能访问自己的
    // Reviewer: 只能访问分配给自己的
    // Editor: 可以访问所有
    // Admin: 无权访问论文内容
    bool check_paper_access(uint32_t paper_id, const Session& session) {
        if (session.role == UserRole::EDITOR) return true; // 编辑拥有最高论文访问权
        if (session.role == UserRole::ADMIN) return false; // 管理员关注系统，不关注业务内容
        
        Paper paper;
        if (!review_manager_->get_paper(paper_id, paper)) return false;

        if (session.role == UserRole::AUTHOR) {
            // 检查是否是作者之一
            for (uint32_t uid : paper.author_ids) {
                if (uid == session.user_id) return true;
            }
        }
        
        if (session.role == UserRole::REVIEWER) {
            // 检查是否被分配
            for (uint32_t rid : paper.reviewer_ids) {
                if (rid == session.user_id) return true;
            }
        }
        
        return false;
    }

public:
    WebApiController(
        std::shared_ptr<FileSystem> fs,
        std::shared_ptr<AuthManager> auth_manager,
        std::shared_ptr<ReviewManager> review_manager
    ) : fs_(fs), auth_manager_(auth_manager), review_manager_(review_manager) {}
    
    // 注册所有 API 路由
    void registerRoutes(WebServer& server) {
        // ========== 认证 API ==========
        server.post("/api/login", [this](const HttpRequest& req) { return handleLogin(req); });
        server.post("/api/logout", [this](const HttpRequest& req) { return handleLogout(req); });
        
        // ========== 用户管理 API (仅管理员) ==========
        server.post("/api/register", [this](const HttpRequest& req) { return handleRegister(req); });
        server.post("/api/users", [this](const HttpRequest& req) { return handleGetUsers(req); });
        server.post("/api/reviewers", [this](const HttpRequest& req) { return handleGetReviewers(req); });
        server.post("/api/update-user", [this](const HttpRequest& req) { return handleUpdateUser(req); });
        server.post("/api/toggle-user", [this](const HttpRequest& req) { return handleToggleUser(req); });
        
        // ========== 论文管理 API ==========
        server.post("/api/papers", [this](const HttpRequest& req) { return handleGetPapers(req); });
        server.post("/api/paper/:id", [this](const HttpRequest& req) { return handleGetPaperDetail(req); });
        server.post("/api/submit", [this](const HttpRequest& req) { return handleSubmitPaper(req); });
        server.post("/api/download/:id", [this](const HttpRequest& req) { return handleDownloadPaper(req); });
        server.post("/api/revise", [this](const HttpRequest& req) { return handleRevisePaper(req); });
        
        // ========== 审稿管理 API ==========
        server.post("/api/assign", [this](const HttpRequest& req) { return handleAssignReviewer(req); });
        server.post("/api/review", [this](const HttpRequest& req) { return handleSubmitReview(req); });
        server.post("/api/decision", [this](const HttpRequest& req) { return handleMakeDecision(req); });
        server.post("/api/my-reviews", [this](const HttpRequest& req) { return handleGetMyReviews(req); });
        
        // ========== 系统管理 API (仅管理员) ==========
        server.post("/api/status", [this](const HttpRequest& req) { return handleGetStatus(req); });
        server.post("/api/backup", [this](const HttpRequest& req) { return handleCreateBackup(req); });
        server.post("/api/backups", [this](const HttpRequest& req) { return handleGetBackups(req); });
    }

private:
    std::string getSessionToken(const HttpRequest& req) {
        auto it = req.headers.find("Authorization");
        if (it != req.headers.end()) {
            if (it->second.substr(0, 7) == "Bearer ") return it->second.substr(7);
            return it->second;
        }
        auto cookie_it = req.cookies.find("session_token");
        if (cookie_it != req.cookies.end()) return cookie_it->second;
        try {
            json body = req.getJson();
            if (body.contains("session_token")) return body["session_token"];
        } catch (...) {}
        return "";
    }
    
    bool validateSession(const HttpRequest& req, Session& session) {
        std::string token = getSessionToken(req);
        if (token.empty()) return false;
        return auth_manager_->validate_session(token, session);
    }

    // ==================== API 处理函数 ====================
    
    HttpResponse handleLogin(const HttpRequest& req) {
        try {
            json body = req.getJson();
            if (!body.contains("username") || !body.contains("password")) return error(400, "Missing credentials");
            
            std::string token = auth_manager_->login(body["username"], body["password"]);
            if (token.empty()) return error(401, "Invalid username or password");
            
            User user;
            auth_manager_->get_user_by_name(body["username"], user);
            
            json result;
            result["success"] = true;
            result["session_token"] = token;
            result["user_id"] = user.user_id;
            result["role"] = static_cast<int>(user.role);
            result["message"] = "Login successful";
            return HttpResponse().setCookie("session_token", token, 86400).json(result);
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    HttpResponse handleLogout(const HttpRequest& req) {
        Session session;
        if (validateSession(req, session)) auth_manager_->logout(getSessionToken(req));
        return HttpResponse().setCookie("session_token", "", 0).json({{"success", true}, {"message", "Logged out"}});
    }
    
    // [权限] 仅管理员
    HttpResponse handleRegister(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            json body = req.getJson();
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");
            UserRole role = static_cast<UserRole>(body.value("role", 4));
            
            if (username.empty() || password.empty()) return error(400, "Missing required fields");
            
            int user_id = auth_manager_->create_user(username, password, role, 
                body.value("email", ""), body.value("full_name", username));
            
            if (user_id <= 0) return error(400, "Failed to create user");
            
            if (role == UserRole::REVIEWER && body.contains("research_areas")) {
                auth_manager_->update_user_research_areas(user_id, body["research_areas"].get<std::vector<std::string>>());
            }
            return success({{"success", true}, {"user_id", user_id}, {"message", "User created"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅管理员
    HttpResponse handleGetUsers(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only"); // 严格限制
        
        try {
            auto users = auth_manager_->get_all_users();
            json users_json = json::array();
            for (const auto& user : users) {
                users_json.push_back({
                    {"id", user.user_id},
                    {"username", user.username},
                    {"email", user.email},
                    {"full_name", user.full_name},
                    {"role", static_cast<int>(user.role)},
                    {"is_active", user.is_active},
                    {"research_areas", user.research_areas}
                });
            }
            return success({{"success", true}, {"users", users_json}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }

    // [权限] 管理员 / 编辑：仅用于分配审稿人等场景，返回审稿人列表
    HttpResponse handleGetReviewers(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN && session.role != UserRole::EDITOR) {
            return error(403, "Admin or Editor only");
        }

        try {
            auto reviewers = auth_manager_->list_users(UserRole::REVIEWER);
            json users_json = json::array();
            for (const auto& user : reviewers) {
                users_json.push_back({
                    {"id", user.user_id},
                    {"username", user.username},
                    {"email", user.email},
                    {"full_name", user.full_name},
                    {"role", static_cast<int>(user.role)},
                    {"is_active", user.is_active},
                    {"research_areas", user.research_areas}
                });
            }
            return success({{"success", true}, {"users", users_json}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅管理员
    HttpResponse handleUpdateUser(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            json body = req.getJson();
            uint32_t user_id = safe_get_id(body, "user_id");
            if (user_id <= 0) return error(400, "Invalid user_id");
            
            User user;
            if (!auth_manager_->get_user(user_id, user)) return error(404, "User not found");

            user.email = body.value("email", user.email);
            user.full_name = body.value("full_name", user.full_name);
            
            bool ok = auth_manager_->update_user(user_id, user);
            std::string pwd = body.value("password", "");
            if (ok && !pwd.empty()) auth_manager_->reset_password(user_id, pwd);
            
            return success({{"success", ok}, {"message", ok ? "Updated" : "Failed"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅管理员
    HttpResponse handleToggleUser(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            json body = req.getJson();
            uint32_t user_id = safe_get_id(body, "user_id");
            if (user_id <= 0) return error(400, "Invalid user_id");
            
            User user;
            if (!auth_manager_->get_user(user_id, user)) return error(404, "User not found");

            user.is_active = !user.is_active;
            bool ok = auth_manager_->update_user(user_id, user);
            return success({{"success", ok}, {"is_active", user.is_active}, {"message", "Status updated"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 分角色查看
    // Author: 仅看自己
    // Reviewer: 仅看分配
    // Editor: 看所有
    // Admin: 无法查看 (遵循职责表)
    HttpResponse handleGetPapers(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        
        try {
            std::vector<Paper> papers;
            
            if (session.role == UserRole::AUTHOR) {
                papers = review_manager_->get_papers_by_author(session.user_id);
            } else if (session.role == UserRole::REVIEWER) {
                // [FIX] 审稿人只能看分配给自己的
                papers = review_manager_->get_reviewer_papers(session.user_id);
            } else if (session.role == UserRole::EDITOR) {
                papers = review_manager_->get_all_papers();
            } else {
                // Admin: Empty list
                papers = {};
            }
            
            json papers_json = json::array();
            for (const auto& paper : papers) {
                papers_json.push_back({
                    {"id", paper.paper_id},
                    {"title", paper.title},
                    {"abstract", paper.abstract},
                    {"authors", join_strings(paper.authors, ", ")},
                    {"keywords", paper.keywords},
                    {"research_area", paper.research_area},
                    {"status", static_cast<int>(paper.status)},
                    {"submit_time", paper.submit_time},
                    {"version", paper.version}
                });
            }
            return success({{"success", true}, {"papers", papers_json}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 访问控制
    HttpResponse handleGetPaperDetail(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        
        try {
            uint32_t paper_id = 0;
            auto it = req.params.find("id");
            if (it != req.params.end()) paper_id = std::stoul(it->second);
            
            // [FIX] 严格检查访问权限
            if (!check_paper_access(paper_id, session)) {
                return error(403, "Access denied");
            }
            
            Paper paper;
            review_manager_->get_paper(paper_id, paper);
            
            json p = {
                {"id", paper.paper_id},
                {"title", paper.title},
                {"abstract", paper.abstract},
                {"authors", join_strings(paper.authors, ", ")},
                {"keywords", paper.keywords},
                {"research_area", paper.research_area},
                {"status", static_cast<int>(paper.status)},
                {"submit_time", paper.submit_time},
                {"submitter_id", paper.submitter_id},
                {"version", paper.version}
            };
            
            auto reviews = review_manager_->get_paper_reviews(paper_id);
            json reviews_json = json::array();
            for (const auto& review : reviews) {
                json r = {
                    {"reviewer_id", review.reviewer_id},
                    {"score", review.score},
                    {"comments", review.comments},
                    {"recommendation", static_cast<int>(review.recommendation)},
                    {"submit_time", review.submit_time}
                };
                
                // [FIX] 只有编辑能看保密意见，Admin不能看
                if (session.role == UserRole::EDITOR) {
                    r["confidential_comments"] = review.confidential_comments;
                }
                reviews_json.push_back(r);
            }
            p["reviews"] = reviews_json;
            return success({{"success", true}, {"paper", p}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅作者
    HttpResponse handleSubmitPaper(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        // [FIX] 严格限制只有作者能提交
        if (session.role != UserRole::AUTHOR) return error(403, "Authors only");
        
        try {
            json body = req.getJson();
            std::string title = body.value("title", "");
            
            std::vector<std::string> authors;
            if (body.contains("authors")) authors = body["authors"].get<std::vector<std::string>>();
            
            std::vector<std::string> keywords;
            if (body.contains("keywords")) keywords = body["keywords"].get<std::vector<std::string>>();
            
            std::vector<uint8_t> content;
            if (body.contains("paper_content")) content = AuthUtils::base64_decode(body["paper_content"]);
            
            if (title.empty()) return error(400, "Title required");
            
            int pid = review_manager_->submit_paper(session.user_id, title, body.value("abstract", ""), 
                                                  authors, keywords, body.value("research_area", ""), content);
            
            if (pid <= 0) return error(500, "Submission failed");
            return success({{"success", true}, {"paper_id", pid}, {"message", "Submitted"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 访问控制
    HttpResponse handleDownloadPaper(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        
        try {
            uint32_t paper_id = 0;
            auto it = req.params.find("id");
            if (it != req.params.end()) paper_id = std::stoul(it->second);
            if (paper_id == 0) return error(400, "Invalid ID");
            
            // [FIX] 严格检查
            if (!check_paper_access(paper_id, session)) return error(403, "Access denied");
            
            std::vector<uint8_t> content;
            if (!review_manager_->download_paper(paper_id, session.user_id, content)) 
                return error(404, "Content not found");
            
            return success({{"success", true}, {"content", AuthUtils::base64_encode(content)}, {"paper_id", paper_id}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅作者
    HttpResponse handleRevisePaper(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::AUTHOR) return error(403, "Authors only");
        
        try {
            json body = req.getJson();
            uint32_t paper_id = safe_get_id(body, "paper_id");
            if (paper_id == 0) return error(400, "Invalid ID");
            
            // 确保是该论文的作者
            if (!check_paper_access(paper_id, session)) return error(403, "Not your paper");

            std::vector<uint8_t> content;
            if (body.contains("paper_content")) content = AuthUtils::base64_decode(body["paper_content"]);
            if (content.empty()) return error(400, "Content required");
            
            bool ok = review_manager_->revise_paper(paper_id, session.user_id, content, body.value("change_description", ""));
            return success({{"success", ok}, {"message", ok ? "Revised" : "Failed"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅编辑 (Admin 移除)
    HttpResponse handleAssignReviewer(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        // [FIX] 仅编辑可分配，管理员不可
        if (session.role != UserRole::EDITOR) return error(403, "Editor only");
        
        try {
            json body = req.getJson();
            uint32_t paper_id = safe_get_id(body, "paper_id");
            bool auto_assign = body.value("auto_assign", false);
            std::vector<uint32_t> reviewer_ids;
            
            if (auto_assign) {
                reviewer_ids = review_manager_->auto_assign_reviewers(paper_id, session.user_id, 3);
            } else if (body.contains("reviewer_ids")) {
                reviewer_ids = body["reviewer_ids"].get<std::vector<uint32_t>>();
            }
            
            if (reviewer_ids.empty()) return error(400, "No reviewers");
            bool ok = review_manager_->assign_reviewers(paper_id, session.user_id, reviewer_ids);
            return success({{"success", ok}, {"assigned_reviewers", reviewer_ids}, {"message", ok ? "Assigned" : "Failed"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅审稿人
    HttpResponse handleSubmitReview(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        // [FIX] 仅审稿人可提交评审 (Editor/Admin 移除)
        if (session.role != UserRole::REVIEWER) return error(403, "Reviewer only");
        
        try {
            json body = req.getJson();
            uint32_t paper_id = safe_get_id(body, "paper_id");
            
            // 检查是否被分配
            if (!check_paper_access(paper_id, session)) return error(403, "Not assigned to this paper");
            
            int rid = review_manager_->submit_review(paper_id, session.user_id, 
                body.value("score", 0), body.value("comments", ""), 
                body.value("confidential_comments", ""), 
                static_cast<DecisionType>(body.value("recommendation", 1)));
            
            if (rid <= 0) return error(500, "Failed");
            return success({{"success", true}, {"review_id", rid}, {"message", "Review submitted"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅编辑 (Admin 移除)
    HttpResponse handleMakeDecision(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        // [FIX] 仅编辑
        if (session.role != UserRole::EDITOR) return error(403, "Editor only");
        
        try {
            json body = req.getJson();
            uint32_t paper_id = safe_get_id(body, "paper_id");
            bool ok = review_manager_->make_decision(paper_id, session.user_id, 
                static_cast<DecisionType>(body.value("decision", 1)), body.value("comments", ""));
            return success({{"success", ok}, {"message", ok ? "Decision recorded" : "Failed"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    HttpResponse handleGetMyReviews(const HttpRequest& req) {
        return handleGetPapers(req); // 复用逻辑
    }
    
    // [权限] 仅管理员
    HttpResponse handleGetStatus(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            auto sb = fs_->get_super_block();
            uint64_t h, m, e;
            fs_->get_cache_statistics(h, m, e);
            return success({
                {"success", true},
                {"total_blocks", sb.total_blocks},
                {"free_blocks", sb.free_blocks},
                {"total_inodes", sb.total_inodes},
                {"free_inodes", sb.free_inodes},
                {"cache_hits", h},
                {"cache_misses", m},
                {"cache_hit_rate", fs_->get_cache_hit_rate()},
                {"active_users", auth_manager_->get_total_users()},
                {"total_papers", review_manager_->get_total_papers()}
            });
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅管理员
    HttpResponse handleCreateBackup(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            const std::string backup_dir = "./backups";
            std::filesystem::create_directories(backup_dir);

            const std::string filename = "snapshot_" + std::to_string(time(nullptr)) + ".bak";
            const std::string full_path = backup_dir + "/" + filename;

            const bool ok = fs_->create_backup(full_path);
            if (!ok) return error(500, "Backup failed");

            return success({{"success", true}, {"backup_path", filename}, {"message", "Backup created"}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    // [权限] 仅管理员
    HttpResponse handleGetBackups(const HttpRequest& req) {
        Session session;
        if (!validateSession(req, session)) return error(401, "Unauthorized");
        if (session.role != UserRole::ADMIN) return error(403, "Admin only");
        
        try {
            const std::string backup_dir = "./backups";
            std::vector<std::string> names;

            if (std::filesystem::exists(backup_dir) && std::filesystem::is_directory(backup_dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(backup_dir)) {
                    if (!entry.is_regular_file()) continue;
                    const std::string name = entry.path().filename().string();
                    if (name.find("snapshot_") != std::string::npos) {
                        names.push_back(name);
                    }
                }
            }

            std::sort(names.begin(), names.end(), std::greater<std::string>());
            json backups = json::array();
            for (const auto& name : names) backups.push_back(name);

            return success({{"success", true}, {"backups", backups}});
        } catch (const std::exception& e) { return error(500, e.what()); }
    }
    
    static std::string join_strings(const std::vector<std::string>& strs, const std::string& del) {
        if (strs.empty()) return "";
        std::string res = strs[0];
        for (size_t i = 1; i < strs.size(); i++) res += del + strs[i];
        return res;
    }
};

// ==================== Web 服务器主程序入口 ====================
class ReviewWebApp {
private:
    std::shared_ptr<FileSystem> fs_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<ReviewManager> review_manager_;
    std::unique_ptr<WebServer> web_server_;
    std::unique_ptr<WebApiController> api_controller_;
    
public:
    ReviewWebApp(const std::string& disk_file, int http_port = 8080, size_t cache_size = 256) {
        fs_ = std::make_shared<FileSystem>(disk_file, cache_size);
        auth_manager_ = std::make_shared<AuthManager>(fs_);
        review_manager_ = std::make_shared<ReviewManager>(fs_, auth_manager_);
        web_server_ = std::make_unique<WebServer>(http_port);
        api_controller_ = std::make_unique<WebApiController>(fs_, auth_manager_, review_manager_);
    }
    
    bool initialize() {
        if (!fs_->mount()) {
            std::cout << "Formatting filesystem..." << std::endl;
            if (!fs_->format()) return false;
            if (!fs_->mount()) return false;
        }
        if (!auth_manager_->initialize()) return false;
        if (!review_manager_->initialize()) return false;
        
        web_server_->setStaticDir("./web");
        web_server_->use(corsMiddleware("*"));
        api_controller_->registerRoutes(*web_server_);
        return true;
    }
    
    void run() {
        std::cout << "Starting Review System Web Server..." << std::endl;
        web_server_->start();
        web_server_->run();
    }
    
    void stop() {
        web_server_->stop();
        fs_->unmount();
    }
};

#endif // WEB_API_CONTROLLER_H