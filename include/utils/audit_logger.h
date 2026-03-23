#ifndef AUDIT_LOGGER_H
#define AUDIT_LOGGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>

// ==================== 日志级别 ====================
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// ==================== 操作类型 ====================
enum class OperationType {
    // 用户操作
    USER_LOGIN,
    USER_LOGOUT,
    USER_CREATE,
    USER_DELETE,
    
    // 论文操作
    PAPER_SUBMIT,
    PAPER_REVISE,
    PAPER_DOWNLOAD,
    PAPER_DELETE,
    
    // 审稿操作
    REVIEWER_ASSIGN,
    REVIEW_SUBMIT,
    DECISION_MAKE,
    
    // 系统操作
    SYSTEM_BACKUP,
    SYSTEM_RESTORE,
    SYSTEM_START,
    SYSTEM_STOP,
    
    // 文件系统操作
    FS_CREATE_FILE,
    FS_DELETE_FILE,
    FS_READ_FILE,
    FS_WRITE_FILE,
    FS_CREATE_DIR
};

// ==================== 审计日志条目 ====================
struct AuditLogEntry {
    uint64_t log_id;
    time_t timestamp;
    uint32_t user_id;
    std::string username;
    std::string client_ip;
    OperationType operation;
    std::string target;          // 操作目标(文件路径/论文ID等)
    std::string details;         // 详细信息
    bool success;
    std::string error_message;
    
    AuditLogEntry() : log_id(0), timestamp(0), user_id(0), 
                      operation(OperationType::USER_LOGIN), success(true) {}
    
    std::string to_string() const;
    std::string to_json() const;
};

// ==================== 文件系统日志条目 (WAL) ====================
enum class FSLogType {
    BEGIN_TRANSACTION,
    END_TRANSACTION,
    WRITE_BLOCK,
    ALLOCATE_BLOCK,
    FREE_BLOCK,
    ALLOCATE_INODE,
    FREE_INODE,
    UPDATE_INODE,
    UPDATE_SUPERBLOCK
};

struct FSLogEntry {
    uint64_t log_seq;           // 日志序列号
    FSLogType type;
    uint64_t block_number;
    uint32_t inode_number;
    std::vector<uint8_t> old_data;
    std::vector<uint8_t> new_data;
    time_t timestamp;
    bool committed;
    
    FSLogEntry() : log_seq(0), type(FSLogType::BEGIN_TRANSACTION),
                   block_number(0), inode_number(0), timestamp(0), committed(false) {}
};

// ==================== 审计日志管理器 ====================
class AuditLogger {
private:
    std::string log_file_path_;
    std::ofstream log_file_;
    mutable std::mutex log_mutex_;
    uint64_t next_log_id_;
    LogLevel min_level_;
    bool enabled_;
    
    // 内存缓存(用于快速查询)
    std::vector<AuditLogEntry> recent_logs_;
    static constexpr size_t MAX_RECENT_LOGS = 1000;
    
public:
    explicit AuditLogger(const std::string& log_path = "audit.log");
    ~AuditLogger();
    
    // 启用/禁用
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    void set_min_level(LogLevel level) { min_level_ = level; }
    
    // 记录审计日志
    void log(uint32_t user_id, const std::string& username,
             OperationType op, const std::string& target,
             bool success, const std::string& details = "",
             const std::string& error = "", const std::string& client_ip = "");
    
    // 快捷方法
    void log_login(uint32_t user_id, const std::string& username, 
                   bool success, const std::string& ip);
    void log_paper_submit(uint32_t user_id, const std::string& username,
                          uint32_t paper_id, const std::string& title);
    void log_review_submit(uint32_t user_id, const std::string& username,
                           uint32_t paper_id, int score);
    void log_decision(uint32_t user_id, const std::string& username,
                      uint32_t paper_id, const std::string& decision);
    
    // 查询日志
    std::vector<AuditLogEntry> get_recent_logs(size_t count = 50) const;
    std::vector<AuditLogEntry> get_logs_by_user(uint32_t user_id, size_t count = 50) const;
    std::vector<AuditLogEntry> get_logs_by_operation(OperationType op, size_t count = 50) const;
    std::vector<AuditLogEntry> get_logs_in_timerange(time_t start, time_t end) const;
    
    // 统计
    struct LogStatistics {
        uint64_t total_logs;
        uint64_t login_count;
        uint64_t failed_logins;
        uint64_t paper_submissions;
        uint64_t reviews_submitted;
        uint64_t decisions_made;
        std::map<OperationType, uint64_t> operation_counts;
    };
    LogStatistics get_statistics() const;
    
    // 导出
    bool export_to_file(const std::string& path, time_t start = 0, time_t end = 0) const;
    
private:
    void write_entry(const AuditLogEntry& entry);
    std::string operation_to_string(OperationType op) const;
};

// ==================== 文件系统日志管理器 (WAL) ====================
class FSJournal {
private:
    std::string journal_path_;
    mutable std::fstream journal_file_;
    mutable std::mutex journal_mutex_;
    uint64_t next_seq_;
    bool recovery_mode_;
    
    // 当前事务
    std::vector<FSLogEntry> current_transaction_;
    bool in_transaction_;
    
    static constexpr size_t MAX_JOURNAL_SIZE = 1024 * 1024;  // 1MB
    
public:
    explicit FSJournal(const std::string& journal_path = "fs.journal");
    ~FSJournal();
    
    // 事务管理
    bool begin_transaction();
    bool commit_transaction();
    bool rollback_transaction();
    bool in_transaction() const { return in_transaction_; }
    
    // 记录操作
    void log_write_block(uint64_t block_num, const std::vector<uint8_t>& old_data,
                        const std::vector<uint8_t>& new_data);
    void log_allocate_block(uint64_t block_num);
    void log_free_block(uint64_t block_num);
    void log_update_inode(uint32_t inode_num, const std::vector<uint8_t>& old_data,
                         const std::vector<uint8_t>& new_data);
    
    // 恢复
    bool needs_recovery() const;
    bool recover(std::function<bool(const FSLogEntry&)> apply_callback);
    
    // 检查点
    bool checkpoint();
    
    // 状态
    uint64_t get_journal_size() const;
    uint64_t get_pending_entries() const;
    
private:
    bool write_log_entry(const FSLogEntry& entry);
    bool read_log_entries(std::vector<FSLogEntry>& entries);
    void truncate_journal();
};

// ==================== 工具函数 ====================
namespace LogUtils {
    std::string level_to_string(LogLevel level);
    std::string time_to_string(time_t t);
    std::string format_log_entry(const AuditLogEntry& entry);
    
    // 彩色输出支持
    std::string colorize(const std::string& text, const std::string& color);
    
    // ANSI颜色代码
    constexpr const char* COLOR_RESET = "\033[0m";
    constexpr const char* COLOR_RED = "\033[31m";
    constexpr const char* COLOR_GREEN = "\033[32m";
    constexpr const char* COLOR_YELLOW = "\033[33m";
    constexpr const char* COLOR_BLUE = "\033[34m";
    constexpr const char* COLOR_MAGENTA = "\033[35m";
    constexpr const char* COLOR_CYAN = "\033[36m";
    constexpr const char* COLOR_WHITE = "\033[37m";
    constexpr const char* COLOR_BOLD = "\033[1m";
}

#endif // AUDIT_LOGGER_H