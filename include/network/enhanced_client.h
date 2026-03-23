#ifndef ENHANCED_CLIENT_H
#define ENHANCED_CLIENT_H

#include "protocol.h"
#include "terminal_ui.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

// ==================== 命令定义 ====================
struct Command {
    std::string name;
    std::string description;
    std::string usage;
    std::vector<std::string> aliases;
    std::function<bool(const std::vector<std::string>&)> handler;
    bool requires_login;
    std::string required_role;  // 空表示任何角色
};

// ==================== 增强版客户端 ====================
class EnhancedClient {
private:
    std::string host_;
    int port_;
    int socket_fd_;
    std::string session_token_;
    std::string current_user_;
    std::string current_role_;
    bool connected_;
    bool color_enabled_;
    
    std::map<std::string, Command> commands_;
    std::vector<std::string> command_history_;
    size_t history_index_;
    
public:
    EnhancedClient(const std::string& host, int port);
    ~EnhancedClient();
    
    // ===== 连接管理 =====
    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }
    
    // ===== 主循环 =====
    void run();
    
    // ===== 设置 =====
    void enable_color(bool enable) { color_enabled_ = enable; }
    
private:
    // ===== 初始化 =====
    void register_commands();
    void print_welcome();
    void print_prompt();
    
    // ===== 命令处理 =====
    bool process_line(const std::string& line);
    std::vector<std::string> parse_args(const std::string& line);
    
    // ===== 通信 =====
    std::vector<uint8_t> send_request(const std::vector<uint8_t>& request);
    
    // ===== 命令实现 =====
    bool cmd_help(const std::vector<std::string>& args);
    bool cmd_login(const std::vector<std::string>& args);
    bool cmd_logout(const std::vector<std::string>& args);
    bool cmd_whoami(const std::vector<std::string>& args);
    bool cmd_clear(const std::vector<std::string>& args);
    bool cmd_exit(const std::vector<std::string>& args);
    
    // 管理员命令
    bool cmd_stat(const std::vector<std::string>& args);
    bool cmd_register(const std::vector<std::string>& args);
    bool cmd_users(const std::vector<std::string>& args);
    bool cmd_backup(const std::vector<std::string>& args);
    bool cmd_logs(const std::vector<std::string>& args);
    
    // 作者命令
    bool cmd_submit(const std::vector<std::string>& args);
    bool cmd_revise(const std::vector<std::string>& args);
    bool cmd_my_papers(const std::vector<std::string>& args);
    
    // 编辑命令
    bool cmd_papers(const std::vector<std::string>& args);
    bool cmd_assign(const std::vector<std::string>& args);
    bool cmd_auto_assign(const std::vector<std::string>& args);
    bool cmd_decide(const std::vector<std::string>& args);
    bool cmd_progress(const std::vector<std::string>& args);
    
    // 审稿人命令
    bool cmd_my_reviews(const std::vector<std::string>& args);
    bool cmd_review(const std::vector<std::string>& args);
    bool cmd_download(const std::vector<std::string>& args);
    
    // 通用命令
    bool cmd_status(const std::vector<std::string>& args);
    bool cmd_notifications(const std::vector<std::string>& args);
    bool cmd_similarity(const std::vector<std::string>& args);
    
    // ===== 显示辅助 =====
    void show_paper_table(const std::vector<std::map<std::string, std::string>>& papers);
    void show_user_table(const std::vector<std::map<std::string, std::string>>& users);
    void show_review_progress(uint32_t paper_id);
    void show_notifications();
    
    // ===== 工具 =====
    std::string colorize(const std::string& text, const char* color);
    void print_error(const std::string& msg);
    void print_success(const std::string& msg);
    void print_info(const std::string& msg);
    void print_warning(const std::string& msg);
};

#endif // ENHANCED_CLIENT_H
