#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "filesystem.h"
#include "protocol.h"
#include <string>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <vector>
#include <ctime>

// ==================== 用户信息 ====================
struct User {
    uint32_t user_id;
    std::string username;
    std::string password_hash;     // SHA-256哈希
    std::string salt;               // 密码盐值
    UserRole role;
    std::string email;
    std::string full_name;
    std::string affiliation;        // 单位
    std::vector<std::string> research_areas;  // 研究领域
    time_t created_time;
    time_t last_login;
    bool is_active;
    uint32_t paper_count;           // 论文数量
    uint32_t review_count;          // 审稿数量
    
    User() : user_id(0), role(UserRole::AUTHOR), 
             created_time(0), last_login(0), is_active(true),
             paper_count(0), review_count(0) {}
};

// ==================== 会话信息 ====================
struct Session {
    std::string session_token;
    uint32_t user_id;
    UserRole role;
    time_t create_time;
    time_t last_access;
    time_t expire_time;
    std::string client_ip;
    bool is_valid;
    
    Session() : user_id(0), role(UserRole::AUTHOR),
                create_time(0), last_access(0), expire_time(0),
                is_valid(false) {}
};

// ==================== 权限定义 ====================
enum class Permission {
    // 文件操作
    READ_FILE,
    WRITE_FILE,
    DELETE_FILE,
    CREATE_DIR,
    
    // 论文操作
    SUBMIT_PAPER,
    REVISE_PAPER,
    DOWNLOAD_PAPER,
    DELETE_PAPER,
    
    // 审稿操作
    SUBMIT_REVIEW,
    DOWNLOAD_REVIEW,
    ASSIGN_REVIEWER,
    MAKE_DECISION,
    
    // 用户管理
    CREATE_USER,
    DELETE_USER,
    UPDATE_USER,
    VIEW_USERS,
    
    // 系统管理
    CREATE_BACKUP,
    RESTORE_BACKUP,
    VIEW_SYSTEM_STATUS,
    VIEW_LOGS
};

// ==================== 认证管理器 ====================
class AuthManager {
private:
    std::shared_ptr<FileSystem> fs_;
    std::map<uint32_t, User> users_;            // 用户ID -> 用户
    std::map<std::string, uint32_t> username_map_;  // 用户名 -> 用户ID
    std::map<std::string, Session> sessions_;   // Token -> 会话
    std::map<UserRole, std::set<Permission>> role_permissions_;  // 角色权限
    
    mutable std::mutex auth_mutex_;
    uint32_t next_user_id_;
    
    static constexpr time_t SESSION_TIMEOUT = 3600 * 24;  // 24小时
    
public:
    explicit AuthManager(std::shared_ptr<FileSystem> fs);
    ~AuthManager();
    
    // ===== 初始化 =====
    bool initialize();
    bool create_default_admin();
    
    // ===== 用户管理 =====
    int create_user(const std::string& username, const std::string& password,
                   UserRole role, const std::string& email = "",
                   const std::string& full_name = "");
    bool delete_user(uint32_t user_id);
    bool update_user(uint32_t user_id, const User& user);
    bool get_user(uint32_t user_id, User& user);
    bool get_user_by_name(const std::string& username, User& user);
    std::vector<User> list_users(UserRole role = static_cast<UserRole>(0));
    
    // ===== 认证 =====
    std::string login(const std::string& username, const std::string& password,
                     const std::string& client_ip = "");
    bool logout(const std::string& session_token);
    bool validate_session(const std::string& session_token, Session& session);
    bool refresh_session(const std::string& session_token);
    
    // ===== 权限检查 =====
    bool check_permission(const std::string& session_token, Permission perm);
    bool check_permission(UserRole role, Permission perm);
    bool is_admin(const std::string& session_token);
    bool is_editor(const std::string& session_token);
    
    // ===== 密码管理 =====
    bool change_password(uint32_t user_id, const std::string& old_password,
                        const std::string& new_password);
    bool reset_password(uint32_t user_id, const std::string& new_password);
    
    // ===== 统计信息 =====
    size_t get_active_sessions() const;
    size_t get_total_users() const;
    std::vector<Session> get_active_sessions_info();
    // 更新用户研究领域
    bool update_user_research_areas(uint32_t user_id, const std::vector<std::string>& areas);
    
    // 获取所有用户
    std::vector<User> get_all_users();
    
    // ===== 持久化 =====
    bool save_users();
    bool load_users();
    
private:
    // ===== 密码处理 =====
    std::string hash_password(const std::string& password, const std::string& salt);
    std::string generate_salt();
    bool verify_password(const std::string& password, const std::string& hash,
                        const std::string& salt);
    
    // ===== 会话管理 =====
    std::string generate_session_token();
    void cleanup_expired_sessions();
    
    // ===== 权限初始化 =====
    void initialize_permissions();
    
    // ===== 用户持久化路径 =====
    std::string get_user_db_path() const { return "/users.db"; }
};

// ==================== COI检测器 ====================
class COIDetector {
private:
    std::shared_ptr<AuthManager> auth_manager_;
    
    // COI规则配置
    struct COIRule {
        bool check_same_affiliation;
        bool check_collaboration;
        bool check_advisor_student;
        uint32_t collaboration_years;  // 检查最近N年的合作
        
        COIRule() : check_same_affiliation(true),
                   check_collaboration(true),
                   check_advisor_student(true),
                   collaboration_years(3) {}
    };
    
    COIRule rules_;
    
public:
    explicit COIDetector(std::shared_ptr<AuthManager> auth_manager);
    
    // COI冲突类型
    enum class ConflictType {
        SAME_AFFILIATION,      // 同单位
        RECENT_COLLABORATION,   // 近期合作
        ADVISOR_STUDENT,        // 师生关系
        SELF_REVIEW            // 自审
    };
    
    struct ConflictInfo {
        ConflictType type;
        std::string reason;
        double severity;  // 严重程度 0-1
    };
    
    // 检测COI
    std::vector<ConflictInfo> detect_conflicts(uint32_t paper_author_id,
                                               uint32_t reviewer_id);
    
    // 批量检测
    std::map<uint32_t, std::vector<ConflictInfo>> 
        detect_conflicts_batch(uint32_t paper_author_id,
                              const std::vector<uint32_t>& reviewer_ids);
    
    // 配置规则
    void set_rules(const COIRule& rules) { rules_ = rules; }
    COIRule get_rules() const { return rules_; }
    
private:
    // 检查同单位
    bool check_same_affiliation(const User& author, const User& reviewer);
    
    // 检查合作关系(简化实现)
    bool check_collaboration(uint32_t author_id, uint32_t reviewer_id);
    
    // 检查师生关系(简化实现)
    bool check_advisor_student(uint32_t author_id, uint32_t reviewer_id);
};

// ==================== 审稿人匹配器 ====================
class ReviewerMatcher {
private:
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<COIDetector> coi_detector_;
    
public:
    ReviewerMatcher(std::shared_ptr<AuthManager> auth_manager,
                   std::shared_ptr<COIDetector> coi_detector);
    
    struct MatchScore {
        uint32_t reviewer_id;
        double score;             // 匹配分数 0-1
        std::string reason;       // 匹配原因
        std::vector<COIDetector::ConflictInfo> coi_conflicts;
    };
    
    // 查找最佳审稿人
    std::vector<MatchScore> find_best_reviewers(
        const std::vector<std::string>& paper_keywords,
        const std::string& research_area,
        uint32_t author_id,
        size_t num_reviewers = 3,
        double min_score = 0.5
    );
    
    // 计算关键词匹配分数
    double calculate_keyword_match(const std::vector<std::string>& paper_keywords,
                                   const std::vector<std::string>& reviewer_areas);
    
    // 考虑审稿负载
    double calculate_load_factor(uint32_t reviewer_id);
    
    // 综合评分
    double calculate_final_score(double keyword_score, double load_factor,
                                double history_score = 0.0);
};

// ==================== 工具函数 ====================
namespace AuthUtils {
    // SHA-256哈希
    std::string sha256(const std::string& input);
    
    // 生成随机字符串
    std::string generate_random_string(size_t length);
    
    // Base64编码/解码
    std::string base64_encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64_decode(const std::string& encoded);
    
    // 验证邮箱格式
    bool validate_email(const std::string& email);
    
    // 验证密码强度
    bool validate_password_strength(const std::string& password);
    
    // 角色转字符串
    std::string role_to_string(UserRole role);
    
    // 字符串转角色
    UserRole string_to_role(const std::string& str);
}

#endif // AUTH_MANAGER_H
