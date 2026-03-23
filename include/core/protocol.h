#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

// JSON库 - 需要安装nlohmann-json3-dev
// Ubuntu/Debian: sudo apt-get install nlohmann-json3-dev
// 或使用: https://github.com/nlohmann/json
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ==================== 协议常量 ====================
constexpr uint32_t PROTOCOL_MAGIC = 0x52455649;     // "REVI"
constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr size_t MAX_MESSAGE_SIZE = 104857600;      // 100MB

// ==================== 消息类型 ====================
enum class MessageType : uint32_t {
    // 认证相关
    LOGIN_REQUEST = 1001,
    LOGIN_RESPONSE = 1002,
    LOGOUT_REQUEST = 1003,
    LOGOUT_RESPONSE = 1004,
    
    // 文件操作
    CREATE_FILE_REQUEST = 2001,
    CREATE_FILE_RESPONSE = 2002,
    CREATE_DIR_REQUEST = 2003,
    CREATE_DIR_RESPONSE = 2004,
    DELETE_REQUEST = 2005,
    DELETE_RESPONSE = 2006,
    RENAME_REQUEST = 2007,
    RENAME_RESPONSE = 2008,
    LIST_DIR_REQUEST = 2009,
    LIST_DIR_RESPONSE = 2010,
    
    // 文件读写
    READ_FILE_REQUEST = 3001,
    READ_FILE_RESPONSE = 3002,
    WRITE_FILE_REQUEST = 3003,
    WRITE_FILE_RESPONSE = 3004,
    
    // 审稿业务
    SUBMIT_PAPER_REQUEST = 4001,
    SUBMIT_PAPER_RESPONSE = 4002,
    REVISE_PAPER_REQUEST = 4003,
    REVISE_PAPER_RESPONSE = 4004,
    DOWNLOAD_PAPER_REQUEST = 4005,
    DOWNLOAD_PAPER_RESPONSE = 4006,
    SUBMIT_REVIEW_REQUEST = 4007,
    SUBMIT_REVIEW_RESPONSE = 4008,
    DOWNLOAD_REVIEW_REQUEST = 4009,
    DOWNLOAD_REVIEW_RESPONSE = 4010,
    ASSIGN_REVIEWER_REQUEST = 4011,
    ASSIGN_REVIEWER_RESPONSE = 4012,
    MAKE_DECISION_REQUEST = 4013,
    MAKE_DECISION_RESPONSE = 4014,
    GET_PAPER_STATUS_REQUEST = 4015,
    GET_PAPER_STATUS_RESPONSE = 4016,
    LIST_PAPERS_REQUEST = 4017,
    LIST_PAPERS_RESPONSE = 4018,
    
    // 用户管理
    CREATE_USER_REQUEST = 5001,
    CREATE_USER_RESPONSE = 5002,
    DELETE_USER_REQUEST = 5003,
    DELETE_USER_RESPONSE = 5004,
    LIST_USERS_REQUEST = 5005,
    LIST_USERS_RESPONSE = 5006,
    UPDATE_USER_REQUEST = 5007,
    UPDATE_USER_RESPONSE = 5008,
    
    // 备份管理
    CREATE_BACKUP_REQUEST = 6001,
    CREATE_BACKUP_RESPONSE = 6002,
    RESTORE_BACKUP_REQUEST = 6003,
    RESTORE_BACKUP_RESPONSE = 6004,
    LIST_BACKUPS_REQUEST = 6005,
    LIST_BACKUPS_RESPONSE = 6006,
    
    // 系统状态
    GET_SYSTEM_STATUS_REQUEST = 7001,
    GET_SYSTEM_STATUS_RESPONSE = 7002,
    
    // 错误
    ERROR_RESPONSE = 9999
};

// ==================== 用户角色 ====================
enum class UserRole : uint32_t {
    ADMIN = 1,
    EDITOR = 2,
    REVIEWER = 3,
    AUTHOR = 4
};

// ==================== 论文状态 ====================
enum class PaperStatus : uint32_t {
    SUBMITTED = 1,
    UNDER_REVIEW = 2,
    REVISION_REQUIRED = 3,
    REVISED = 4,
    ACCEPTED = 5,
    REJECTED = 6
};

// ==================== 决策类型 ====================
enum class DecisionType : uint32_t {
    ACCEPT = 1,
    REJECT = 2,
    MAJOR_REVISION = 3,
    MINOR_REVISION = 4
};

// ==================== 消息头 ====================
struct MessageHeader {
    uint32_t magic;             // 魔数
    uint32_t version;           // 协议版本
    MessageType type;           // 消息类型
    uint32_t sequence;          // 序列号
    uint64_t payload_size;      // 负载大小
    uint64_t timestamp;         // 时间戳
    uint8_t reserved[16];       // 保留
    
    MessageHeader() : magic(PROTOCOL_MAGIC), version(PROTOCOL_VERSION),
                     type(MessageType::ERROR_RESPONSE), sequence(0),
                     payload_size(0), timestamp(0) {
        memset(reserved, 0, sizeof(reserved));
    }
    
    // 序列化为字节数组
    std::vector<uint8_t> serialize() const;
    
    // 从字节数组反序列化
    static MessageHeader deserialize(const std::vector<uint8_t>& data);
};

// ==================== 基础消息 ====================
class Message {
protected:
    MessageHeader header_;
    json payload_;
    
public:
    Message() = default;
    explicit Message(MessageType type) {
        header_.type = type;
        header_.timestamp = time(nullptr);
    }
    
    virtual ~Message() = default;
    
    // 获取消息类型
    MessageType get_type() const { return header_.type; }
    
    // 设置序列号
    void set_sequence(uint32_t seq) { header_.sequence = seq; }
    uint32_t get_sequence() const { return header_.sequence; }
    
    // 序列化整个消息
    std::vector<uint8_t> serialize();
    
    // 从字节数组创建消息
    static std::unique_ptr<Message> deserialize(const std::vector<uint8_t>& data);
    
    // 获取负载
    const json& get_payload() const { return payload_; }
    void set_payload(const json& payload) { payload_ = payload; }
    
protected:
    virtual void build_payload() {}
    virtual void parse_payload() {}
};

// ==================== 登录请求 ====================
class LoginRequest : public Message {
public:
    std::string username;
    std::string password;
    
    LoginRequest() : Message(MessageType::LOGIN_REQUEST) {}
    
    LoginRequest(const std::string& user, const std::string& pass)
        : Message(MessageType::LOGIN_REQUEST), username(user), password(pass) {
        build_payload();
    }
    
protected:
    void build_payload() override {
        payload_["username"] = username;
        payload_["password"] = password;
    }
    
    void parse_payload() override {
        username = payload_["username"];
        password = payload_["password"];
    }
};

// ==================== 登录响应 ====================
class LoginResponse : public Message {
public:
    bool success;
    std::string session_token;
    UserRole role;
    uint32_t user_id;
    std::string message;
    
    LoginResponse() : Message(MessageType::LOGIN_RESPONSE), 
                     success(false), role(UserRole::AUTHOR), user_id(0) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["session_token"] = session_token;
        payload_["role"] = static_cast<uint32_t>(role);
        payload_["user_id"] = user_id;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_["success"];
        session_token = payload_["session_token"];
        role = static_cast<UserRole>(payload_["role"].get<uint32_t>());
        user_id = payload_["user_id"];
        message = payload_["message"];
    }
};

// ==================== 登出请求 ====================
class LogoutRequest : public Message {
public:
    std::string session_token;
    
    LogoutRequest() : Message(MessageType::LOGOUT_REQUEST) {}
    
    LogoutRequest(const std::string& token)
        : Message(MessageType::LOGOUT_REQUEST), session_token(token) {
        build_payload();
    }
    
protected:
    void build_payload() override {
        payload_["session_token"] = session_token;
    }
    
    void parse_payload() override {
        session_token = payload_.value("session_token", "");
    }
};

// ==================== 登出响应 ====================
class LogoutResponse : public Message {
public:
    bool success;
    std::string message;
    
    LogoutResponse() : Message(MessageType::LOGOUT_RESPONSE), success(false) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_.value("success", false);
        message = payload_.value("message", "");
    }
};

// ==================== 提交论文请求 ====================
class SubmitPaperRequest : public Message {
public:
    std::string session_token;
    std::string title;
    std::string abstract;
    std::vector<std::string> authors;
    std::vector<std::string> keywords;
    std::string research_area;
    std::vector<uint8_t> paper_content;  // PDF内容
    
    SubmitPaperRequest() : Message(MessageType::SUBMIT_PAPER_REQUEST) {}
    
protected:
    void build_payload() override {
        payload_["session_token"] = session_token;
        payload_["title"] = title;
        payload_["abstract"] = abstract;
        payload_["authors"] = authors;
        payload_["keywords"] = keywords;
        payload_["research_area"] = research_area;
        
        // Base64编码论文内容
        payload_["paper_content"] = base64_encode(paper_content);
    }
    
    void parse_payload() override {
        session_token = payload_["session_token"];
        title = payload_["title"];
        abstract = payload_["abstract"];
        authors = payload_["authors"].get<std::vector<std::string>>();
        keywords = payload_["keywords"].get<std::vector<std::string>>();
        research_area = payload_["research_area"];
        
        // Base64解码论文内容
        std::string encoded = payload_["paper_content"];
        paper_content = base64_decode(encoded);
    }
    
private:
    static std::string base64_encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64_decode(const std::string& encoded);
};

// ==================== 提交论文响应 ====================
class SubmitPaperResponse : public Message {
public:
    bool success;
    uint32_t paper_id;
    std::string paper_path;
    std::string message;
    
    SubmitPaperResponse() : Message(MessageType::SUBMIT_PAPER_RESPONSE),
                           success(false), paper_id(0) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["paper_id"] = paper_id;
        payload_["paper_path"] = paper_path;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_["success"];
        paper_id = payload_["paper_id"];
        paper_path = payload_["paper_path"];
        message = payload_["message"];
    }
};

// ==================== 分配审稿人请求 ====================
class AssignReviewerRequest : public Message {
public:
    std::string session_token;
    uint32_t paper_id;
    std::vector<uint32_t> reviewer_ids;
    bool auto_assign;  // 自动分配
    
    AssignReviewerRequest() : Message(MessageType::ASSIGN_REVIEWER_REQUEST),
                             paper_id(0), auto_assign(false) {}
    
protected:
    void build_payload() override {
        payload_["session_token"] = session_token;
        payload_["paper_id"] = paper_id;
        payload_["reviewer_ids"] = reviewer_ids;
        payload_["auto_assign"] = auto_assign;
    }
    
    void parse_payload() override {
        session_token = payload_["session_token"];
        paper_id = payload_["paper_id"];
        reviewer_ids = payload_["reviewer_ids"].get<std::vector<uint32_t>>();
        auto_assign = payload_["auto_assign"];
    }
};

// ==================== 分配审稿人响应 ====================
class AssignReviewerResponse : public Message {
public:
    bool success;
    std::vector<uint32_t> assigned_reviewers;
    std::map<uint32_t, std::string> coi_warnings;  // COI冲突警告
    std::string message;
    
    AssignReviewerResponse() : Message(MessageType::ASSIGN_REVIEWER_RESPONSE),
                              success(false) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["assigned_reviewers"] = assigned_reviewers;
        payload_["coi_warnings"] = coi_warnings;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_["success"];
        assigned_reviewers = payload_["assigned_reviewers"].get<std::vector<uint32_t>>();
        coi_warnings = payload_["coi_warnings"].get<std::map<uint32_t, std::string>>();
        message = payload_["message"];
    }
};

// ==================== 论文状态请求 ====================
class GetPaperStatusRequest : public Message {
public:
    std::string session_token;
    uint32_t paper_id;
    
    GetPaperStatusRequest() : Message(MessageType::GET_PAPER_STATUS_REQUEST),
                             paper_id(0) {}
    
protected:
    void build_payload() override {
        payload_["session_token"] = session_token;
        payload_["paper_id"] = paper_id;
    }
    
    void parse_payload() override {
        session_token = payload_["session_token"];
        paper_id = payload_["paper_id"];
    }
};

// ==================== 论文状态响应 ====================
class GetPaperStatusResponse : public Message {
public:
    bool success;
    uint32_t paper_id;
    std::string title;
    PaperStatus status;
    std::vector<std::string> reviewers;
    uint32_t reviews_completed;
    uint32_t reviews_total;
    std::string decision;
    std::string message;
    
    GetPaperStatusResponse() : Message(MessageType::GET_PAPER_STATUS_RESPONSE),
                              success(false), paper_id(0),
                              status(PaperStatus::SUBMITTED),
                              reviews_completed(0), reviews_total(0) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["paper_id"] = paper_id;
        payload_["title"] = title;
        payload_["status"] = static_cast<uint32_t>(status);
        payload_["reviewers"] = reviewers;
        payload_["reviews_completed"] = reviews_completed;
        payload_["reviews_total"] = reviews_total;
        payload_["decision"] = decision;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_["success"];
        paper_id = payload_["paper_id"];
        title = payload_["title"];
        status = static_cast<PaperStatus>(payload_["status"].get<uint32_t>());
        reviewers = payload_["reviewers"].get<std::vector<std::string>>();
        reviews_completed = payload_["reviews_completed"];
        reviews_total = payload_["reviews_total"];
        decision = payload_["decision"];
        message = payload_["message"];
    }
};

// ==================== 系统状态请求 ====================
class GetSystemStatusRequest : public Message {
public:
    std::string session_token;
    
    GetSystemStatusRequest() : Message(MessageType::GET_SYSTEM_STATUS_REQUEST) {}
    
protected:
    void build_payload() override {
        payload_["session_token"] = session_token;
    }
    
    void parse_payload() override {
        session_token = payload_["session_token"];
    }
};

// ==================== 系统状态响应 ====================
class GetSystemStatusResponse : public Message {
public:
    bool success;
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint64_t total_inodes;
    uint64_t free_inodes;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double cache_hit_rate;
    uint32_t active_users;
    uint32_t total_papers;
    std::string uptime;
    std::string message;
    
    GetSystemStatusResponse() : Message(MessageType::GET_SYSTEM_STATUS_RESPONSE),
                               success(false), total_blocks(0), free_blocks(0),
                               total_inodes(0), free_inodes(0), cache_hits(0),
                               cache_misses(0), cache_hit_rate(0.0),
                               active_users(0), total_papers(0) {}
    
protected:
    void build_payload() override {
        payload_["success"] = success;
        payload_["total_blocks"] = total_blocks;
        payload_["free_blocks"] = free_blocks;
        payload_["total_inodes"] = total_inodes;
        payload_["free_inodes"] = free_inodes;
        payload_["cache_hits"] = cache_hits;
        payload_["cache_misses"] = cache_misses;
        payload_["cache_hit_rate"] = cache_hit_rate;
        payload_["active_users"] = active_users;
        payload_["total_papers"] = total_papers;
        payload_["uptime"] = uptime;
        payload_["message"] = message;
    }
    
    void parse_payload() override {
        success = payload_["success"];
        total_blocks = payload_["total_blocks"];
        free_blocks = payload_["free_blocks"];
        total_inodes = payload_["total_inodes"];
        free_inodes = payload_["free_inodes"];
        cache_hits = payload_["cache_hits"];
        cache_misses = payload_["cache_misses"];
        cache_hit_rate = payload_["cache_hit_rate"];
        active_users = payload_["active_users"];
        total_papers = payload_["total_papers"];
        uptime = payload_["uptime"];
        message = payload_["message"];
    }
};

// ==================== 错误响应 ====================
class ErrorResponse : public Message {
public:
    int32_t error_code;
    std::string error_message;
    
    ErrorResponse() : Message(MessageType::ERROR_RESPONSE), error_code(0) {}
    
    ErrorResponse(int32_t code, const std::string& msg)
        : Message(MessageType::ERROR_RESPONSE), error_code(code), error_message(msg) {
        build_payload();
    }
    
protected:
    void build_payload() override {
        payload_["error_code"] = error_code;
        payload_["error_message"] = error_message;
    }
    
    void parse_payload() override {
        error_code = payload_["error_code"];
        error_message = payload_["error_message"];
    }
};

// ==================== 工具函数 ====================
namespace ProtocolUtils {
    // 角色转字符串
    std::string role_to_string(UserRole role);
    
    // 状态转字符串
    std::string status_to_string(PaperStatus status);
    
    // 决策转字符串
    std::string decision_to_string(DecisionType decision);
    
    // 生成会话令牌
    std::string generate_session_token();
    
    // 验证会话令牌
    bool validate_session_token(const std::string& token);
}

#endif // PROTOCOL_H