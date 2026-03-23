#include "auth_manager.h"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <iostream>

// ==================== SHA-256实现 ====================
namespace AuthUtils {
    std::string sha256(const std::string& input) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, input.c_str(), input.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
    
    std::string generate_random_string(size_t length) {
        static const char chars[] = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }
    
    std::string base64_encode(const std::vector<uint8_t>& data) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        
        std::string ret;
        int val = 0;
        int valb = -6;
        
        for (uint8_t c : data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                ret.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) {
            ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }
        
        while (ret.size() % 4) {
            ret.push_back('=');
        }
        
        return ret;
    }
    
    std::vector<uint8_t> base64_decode(const std::string& encoded) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        
        std::vector<uint8_t> ret;
        int val = 0;
        int valb = -8;
        
        for (char c : encoded) {
            if (c == '=') break;
            size_t pos = base64_chars.find(c);
            if (pos == std::string::npos) continue;
            
            val = (val << 6) + pos;
            valb += 6;
            if (valb >= 0) {
                ret.push_back((val >> valb) & 0xFF);
                valb -= 8;
            }
        }
        
        return ret;
    }
    
    bool validate_email(const std::string& email) {
        return email.find('@') != std::string::npos && email.find('.') != std::string::npos;
    }
    
    bool validate_password_strength(const std::string& password) {
        return password.length() >= 6;
    }
    
    std::string role_to_string(UserRole role) {
        switch (role) {
            case UserRole::ADMIN: return "ADMIN";
            case UserRole::EDITOR: return "EDITOR";
            case UserRole::REVIEWER: return "REVIEWER";
            case UserRole::AUTHOR: return "AUTHOR";
            default: return "UNKNOWN";
        }
    }
    
    UserRole string_to_role(const std::string& str) {
        if (str == "ADMIN") return UserRole::ADMIN;
        if (str == "EDITOR") return UserRole::EDITOR;
        if (str == "REVIEWER") return UserRole::REVIEWER;
        if (str == "AUTHOR") return UserRole::AUTHOR;
        return UserRole::AUTHOR;
    }
}

// ==================== AuthManager实现 ====================
AuthManager::AuthManager(std::shared_ptr<FileSystem> fs)
    : fs_(fs), next_user_id_(1) {
}

AuthManager::~AuthManager() {
    save_users();
}

bool AuthManager::initialize() {
    std::cout << "[AUTH] Initializing permissions..." << std::endl;
    initialize_permissions();
    
    std::cout << "[AUTH] Loading users from database..." << std::endl;
    if (!load_users()) {
        std::cout << "[AUTH] No user database found, creating default admin..." << std::endl;
        // 如果没有用户数据，创建默认管理员
        return create_default_admin();
    }
    
    std::cout << "[AUTH] Loaded " << users_.size() << " users" << std::endl;
    
    return true;
}

bool AuthManager::create_default_admin() {
    std::cout << "[AUTH] Creating default admin user..." << std::endl;
    int admin_id = create_user("admin", "admin123", UserRole::ADMIN, 
                               "admin@system.local", "System Administrator");
    
    if (admin_id >= 0) {
        std::cout << "[AUTH] Default admin created with ID: " << admin_id << std::endl;
        return true;
    } else {
        std::cout << "[AUTH] Failed to create default admin" << std::endl;
        return false;
    }
}

int AuthManager::create_user(const std::string& username, const std::string& password,
                             UserRole role, const std::string& email,
                             const std::string& full_name) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    // 检查用户名是否已存在
    if (username_map_.find(username) != username_map_.end()) {
        return -1;
    }
    
    // 创建用户
    User user;
    user.user_id = next_user_id_++;
    user.username = username;
    user.salt = generate_salt();
    user.password_hash = hash_password(password, user.salt);
    user.role = role;
    user.email = email;
    user.full_name = full_name;
    user.created_time = time(nullptr);
    user.is_active = true;
    
    // 为审稿人设置默认研究领域
    if (role == UserRole::REVIEWER) {
        user.research_areas = {"general", "computer science"};
    }
    
    users_[user.user_id] = user;
    username_map_[username] = user.user_id;
    
    save_users();
    
    return user.user_id;
}

bool AuthManager::delete_user(uint32_t user_id) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return false;
    }
    
    username_map_.erase(it->second.username);
    users_.erase(it);
    
    save_users();
    return true;
}

// [新增实现] 更新用户信息
bool AuthManager::update_user(uint32_t user_id, const User& user) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return false;
    }
    
    // 如果用户名改变了（虽然WebAPI目前不改用户名，但为了健壮性）
    if (it->second.username != user.username) {
        if (username_map_.find(user.username) != username_map_.end()) {
            return false; // 新用户名已存在
        }
        username_map_.erase(it->second.username);
        username_map_[user.username] = user_id;
    }
    
    // 更新用户数据
    users_[user_id] = user;
    
    save_users();
    return true;
}

// [新增实现] 重置密码
bool AuthManager::reset_password(uint32_t user_id, const std::string& new_password) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return false;
    }
    
    // 生成新盐值和哈希
    it->second.salt = generate_salt();
    it->second.password_hash = hash_password(new_password, it->second.salt);
    
    save_users();
    return true;
}

bool AuthManager::get_user(uint32_t user_id, User& user) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return false;
    }
    
    user = it->second;
    return true;
}

bool AuthManager::get_user_by_name(const std::string& username, User& user) {
    std::lock_guard<std::mutex> lock(auth_mutex_); 
    
    auto it = username_map_.find(username);
    if (it == username_map_.end()) {
        return false;
    }
    
    auto user_it = users_.find(it->second);
    if (user_it != users_.end()) {
        user = user_it->second;
        return true;
    }
    
    return false;
}

std::vector<User> AuthManager::list_users(UserRole role) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    std::vector<User> result;
    for (const auto& pair : users_) {
        if (role == static_cast<UserRole>(0) || pair.second.role == role) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

// [新增实现] 获取所有用户
std::vector<User> AuthManager::get_all_users() {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    std::vector<User> result;
    for (const auto& pair : users_) {
        result.push_back(pair.second);
    }
    return result;
}

// [新增实现] 更新研究领域
bool AuthManager::update_user_research_areas(uint32_t user_id, const std::vector<std::string>& areas) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return false;
    }
    
    it->second.research_areas = areas;
    save_users();
    return true;
}

std::string AuthManager::login(const std::string& username, const std::string& password,
                               const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    std::cout << "[AUTH] Login attempt for user: " << username << std::endl;
    
    // 查找用户
    auto it = username_map_.find(username);
    if (it == username_map_.end()) {
        std::cout << "[AUTH] User not found: " << username << std::endl;
        return "";
    }
    
    std::cout << "[AUTH] User found, verifying password..." << std::endl;
    
    User& user = users_[it->second];
    
    // 验证密码
    if (!verify_password(password, user.password_hash, user.salt)) {
        std::cout << "[AUTH] Password verification failed" << std::endl;
        return "";
    }
    
    std::cout << "[AUTH] Password verified, creating session..." << std::endl;
    
    // 创建会话
    Session session;
    session.session_token = generate_session_token();
    session.user_id = user.user_id;
    session.role = user.role;
    session.create_time = time(nullptr);
    session.last_access = session.create_time;
    session.expire_time = session.create_time + SESSION_TIMEOUT;
    session.client_ip = client_ip;
    session.is_valid = true;
    
    sessions_[session.session_token] = session;
    
    // 更新用户最后登录时间
    user.last_login = time(nullptr);
    
    std::cout << "[AUTH] Session created: " << session.session_token.substr(0, 8) << "..." << std::endl;
    
    return session.session_token;
}

bool AuthManager::logout(const std::string& session_token) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = sessions_.find(session_token);
    if (it == sessions_.end()) {
        return false;
    }
    
    it->second.is_valid = false;
    sessions_.erase(it);
    
    return true;
}

bool AuthManager::validate_session(const std::string& session_token, Session& session) {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    auto it = sessions_.find(session_token);
    if (it == sessions_.end()) {
        return false;
    }
    
    Session& s = it->second;
    
    // 检查是否过期
    time_t now = time(nullptr);
    if (now > s.expire_time) {
        sessions_.erase(it);
        return false;
    }
    
    // 更新最后访问时间
    s.last_access = now;
    session = s;
    
    return s.is_valid;
}

bool AuthManager::check_permission(const std::string& session_token, Permission perm) {
    Session session;
    if (!validate_session(session_token, session)) {
        return false;
    }
    
    return check_permission(session.role, perm);
}

bool AuthManager::check_permission(UserRole role, Permission perm) {
    auto it = role_permissions_.find(role);
    if (it == role_permissions_.end()) {
        return false;
    }
    
    return it->second.find(perm) != it->second.end();
}

std::string AuthManager::hash_password(const std::string& password, const std::string& salt) {
    return AuthUtils::sha256(password + salt);
}

std::string AuthManager::generate_salt() {
    return AuthUtils::generate_random_string(32);
}

bool AuthManager::verify_password(const std::string& password, const std::string& hash,
                                  const std::string& salt) {
    return hash_password(password, salt) == hash;
}

std::string AuthManager::generate_session_token() {
    return AuthUtils::generate_random_string(64);
}

void AuthManager::initialize_permissions() {
    // 管理员权限
    role_permissions_[UserRole::ADMIN] = {
        Permission::READ_FILE, Permission::WRITE_FILE, Permission::DELETE_FILE, Permission::CREATE_DIR,
        Permission::SUBMIT_PAPER, Permission::REVISE_PAPER, Permission::DOWNLOAD_PAPER, Permission::DELETE_PAPER,
        Permission::SUBMIT_REVIEW, Permission::DOWNLOAD_REVIEW, Permission::ASSIGN_REVIEWER, Permission::MAKE_DECISION,
        Permission::CREATE_USER, Permission::DELETE_USER, Permission::UPDATE_USER, Permission::VIEW_USERS,
        Permission::CREATE_BACKUP, Permission::RESTORE_BACKUP, Permission::VIEW_SYSTEM_STATUS, Permission::VIEW_LOGS
    };
    
    // 编辑权限
    role_permissions_[UserRole::EDITOR] = {
        Permission::READ_FILE, Permission::DOWNLOAD_PAPER,
        Permission::DOWNLOAD_REVIEW, Permission::ASSIGN_REVIEWER, Permission::MAKE_DECISION,
        Permission::VIEW_USERS
    };
    
    // 审稿人权限
    role_permissions_[UserRole::REVIEWER] = {
        Permission::READ_FILE, Permission::DOWNLOAD_PAPER,
        Permission::SUBMIT_REVIEW
    };
    
    // 作者权限
    role_permissions_[UserRole::AUTHOR] = {
        Permission::READ_FILE, Permission::WRITE_FILE,
        Permission::SUBMIT_PAPER, Permission::REVISE_PAPER, Permission::DOWNLOAD_PAPER,
        Permission::DOWNLOAD_REVIEW
    };
}

bool AuthManager::save_users() {
    if (!fs_) {
        std::cout << "[AUTH] save_users: filesystem is null" << std::endl;
        return false;
    }
    if (!fs_->exists(get_user_db_path())) {
        std::cout << "[AUTH] Creating user database file: " << get_user_db_path() << std::endl;
        if (fs_->create_file(get_user_db_path()) < 0) {
            std::cerr << "[AUTH] Failed to create user database file!" << std::endl;
            return false;
        }
    }
    std::cout << "[AUTH] Saving " << users_.size() << " users to database..." << std::endl;
    
    json j;
    j["next_user_id"] = next_user_id_;
    
    json users_array = json::array();
    for (const auto& pair : users_) {
        const User& user = pair.second;
        json user_json;
        user_json["user_id"] = user.user_id;
        user_json["username"] = user.username;
        user_json["password_hash"] = user.password_hash;
        user_json["salt"] = user.salt;
        user_json["role"] = static_cast<int>(user.role);
        user_json["email"] = user.email;
        user_json["full_name"] = user.full_name;
        user_json["affiliation"] = user.affiliation;
        user_json["research_areas"] = user.research_areas;
        user_json["created_time"] = user.created_time;
        user_json["last_login"] = user.last_login;
        user_json["is_active"] = user.is_active;
        
        users_array.push_back(user_json);
    }
    j["users"] = users_array;
    
    std::string data = j.dump();
    std::vector<uint8_t> bytes(data.begin(), data.end());
    
    std::cout << "[AUTH] Writing " << bytes.size() << " bytes to " << get_user_db_path() << std::endl;
    
    int result = fs_->write_file(get_user_db_path(), bytes, 0, false);
    
    if (result >= 0) {
        std::cout << "[AUTH] Users saved successfully" << std::endl;
        return true;
    } else {
        std::cout << "[AUTH] Failed to save users, error code: " << result << std::endl;
        return false;
    }
}

bool AuthManager::load_users() {
    if (!fs_) return false;
    
    if (!fs_->exists(get_user_db_path())) {
        return false;
    }
    
    std::vector<uint8_t> bytes;
    if (fs_->read_file(get_user_db_path(), bytes) < 0) {
        return false;
    }
    
    std::string data(bytes.begin(), bytes.end());
    
    try {
        json j = json::parse(data);
        
        next_user_id_ = j["next_user_id"];
        
        users_.clear();
        username_map_.clear();
        
        for (const auto& user_json : j["users"]) {
            User user;
            user.user_id = user_json["user_id"];
            user.username = user_json["username"];
            user.password_hash = user_json["password_hash"];
            user.salt = user_json["salt"];
            user.role = static_cast<UserRole>(user_json["role"].get<int>());
            user.email = user_json["email"];
            user.full_name = user_json["full_name"];
            user.affiliation = user_json["affiliation"];
            user.research_areas = user_json["research_areas"].get<std::vector<std::string>>();
            user.created_time = user_json["created_time"];
            user.last_login = user_json["last_login"];
            user.is_active = user_json["is_active"];
            
            users_[user.user_id] = user;
            username_map_[user.username] = user.user_id;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

// [新增实现] 检查是否是管理员
bool AuthManager::is_admin(const std::string& session_token) {
    Session session;
    if (!validate_session(session_token, session)) {
        return false;
    }
    return session.role == UserRole::ADMIN;
}

// [新增实现] 检查是否是编辑
bool AuthManager::is_editor(const std::string& session_token) {
    Session session;
    if (!validate_session(session_token, session)) {
        return false;
    }
    return session.role == UserRole::EDITOR;
}

// [新增实现] 获取总用户数
size_t AuthManager::get_total_users() const {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    return users_.size();
}

// [新增实现] 获取活跃会话数
size_t AuthManager::get_active_sessions() const {
    std::lock_guard<std::mutex> lock(auth_mutex_);
    return sessions_.size();
}

// ==================== COIDetector实现 ====================
COIDetector::COIDetector(std::shared_ptr<AuthManager> auth_manager)
    : auth_manager_(auth_manager) {
}

std::vector<COIDetector::ConflictInfo> COIDetector::detect_conflicts(
    uint32_t paper_author_id, uint32_t reviewer_id) {
    
    std::vector<ConflictInfo> conflicts;
    
    User author, reviewer;
    if (!auth_manager_->get_user(paper_author_id, author) ||
        !auth_manager_->get_user(reviewer_id, reviewer)) {
        return conflicts;
    }
    
    // 自审检测
    if (paper_author_id == reviewer_id) {
        ConflictInfo info;
        info.type = ConflictType::SELF_REVIEW;
        info.reason = "Cannot review own paper";
        info.severity = 1.0;
        conflicts.push_back(info);
        return conflicts;
    }
    
    // 同单位检测
    if (check_same_affiliation(author, reviewer)) {
        ConflictInfo info;
        info.type = ConflictType::SAME_AFFILIATION;
        info.reason = "Same affiliation: " + author.affiliation;
        info.severity = 0.9;
        conflicts.push_back(info);
    }
    
    return conflicts;
}

bool COIDetector::check_same_affiliation(const User& author, const User& reviewer) {
    if (author.affiliation.empty() || reviewer.affiliation.empty()) {
        return false;
    }
    return author.affiliation == reviewer.affiliation;
}

// ==================== ReviewerMatcher实现 ====================
ReviewerMatcher::ReviewerMatcher(std::shared_ptr<AuthManager> auth_manager,
                                 std::shared_ptr<COIDetector> coi_detector)
    : auth_manager_(auth_manager), coi_detector_(coi_detector) {
}

std::vector<ReviewerMatcher::MatchScore> ReviewerMatcher::find_best_reviewers(
    const std::vector<std::string>& paper_keywords,
    const std::string& research_area,
    uint32_t author_id,
    size_t num_reviewers,
    double min_score) {
    
    std::vector<MatchScore> scores;
    
    // 获取所有审稿人
    auto reviewers = auth_manager_->list_users(UserRole::REVIEWER);
    
    for (const auto& reviewer : reviewers) {
        // COI检测
        auto conflicts = coi_detector_->detect_conflicts(author_id, reviewer.user_id);
        
        // 如果有严重冲突则跳过
        bool has_severe_conflict = false;
        for (const auto& conflict : conflicts) {
            if (conflict.severity >= 0.9) {
                has_severe_conflict = true;
                break;
            }
        }
        if (has_severe_conflict) continue;
        
        // 计算匹配分数
        double keyword_score = calculate_keyword_match(paper_keywords, reviewer.research_areas);
        double load_factor = calculate_load_factor(reviewer.user_id);
        double final_score = calculate_final_score(keyword_score, load_factor);
        
        if (final_score >= min_score) {
            MatchScore ms;
            ms.reviewer_id = reviewer.user_id;
            ms.score = final_score;
            ms.reason = "Keyword match: " + std::to_string(keyword_score);
            ms.coi_conflicts = conflicts;
            scores.push_back(ms);
        }
    }
    
    // 排序
    std::sort(scores.begin(), scores.end(), 
              [](const MatchScore& a, const MatchScore& b) {
                  return a.score > b.score;
              });
    
    // 返回前N个
    if (scores.size() > num_reviewers) {
        scores.resize(num_reviewers);
    }
    
    return scores;
}

double ReviewerMatcher::calculate_keyword_match(
    const std::vector<std::string>& paper_keywords,
    const std::vector<std::string>& reviewer_areas) {
    
    if (paper_keywords.empty() || reviewer_areas.empty()) {
        return 0.0;
    }
    
    size_t matches = 0;
    for (const auto& pk : paper_keywords) {
        for (const auto& ra : reviewer_areas) {
            if (pk == ra) {
                matches++;
                break;
            }
        }
    }
    
    return static_cast<double>(matches) / paper_keywords.size();
}

double ReviewerMatcher::calculate_load_factor(uint32_t reviewer_id) {
    // 简化实现，实际应查询当前审稿负载
    return 0.8;
}

double ReviewerMatcher::calculate_final_score(double keyword_score, double load_factor,
                                              double history_score) {
    return 0.5 * keyword_score + 0.3 * load_factor + 0.2 * history_score;
}