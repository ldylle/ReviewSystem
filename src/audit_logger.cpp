#include "audit_logger.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <map>
// ==================== AuditLogEntry 实现 ====================
std::string AuditLogEntry::to_string() const {
    std::stringstream ss;
    ss << "[" << LogUtils::time_to_string(timestamp) << "] ";
    ss << "User(" << user_id << ":" << username << ") ";
    ss << "OP=" << static_cast<int>(operation) << " ";
    ss << "Target=" << target << " ";
    ss << (success ? "SUCCESS" : "FAILED");
    if (!error_message.empty()) {
        ss << " Error: " << error_message;
    }
    return ss.str();
}

std::string AuditLogEntry::to_json() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"log_id\":" << log_id << ",";
    ss << "\"timestamp\":" << timestamp << ",";
    ss << "\"user_id\":" << user_id << ",";
    ss << "\"username\":\"" << username << "\",";
    ss << "\"client_ip\":\"" << client_ip << "\",";
    ss << "\"operation\":" << static_cast<int>(operation) << ",";
    ss << "\"target\":\"" << target << "\",";
    ss << "\"details\":\"" << details << "\",";
    ss << "\"success\":" << (success ? "true" : "false") << ",";
    ss << "\"error\":\"" << error_message << "\"";
    ss << "}";
    return ss.str();
}

// ==================== AuditLogger 实现 ====================
AuditLogger::AuditLogger(const std::string& log_path)
    : log_file_path_(log_path), next_log_id_(1), 
      min_level_(LogLevel::INFO), enabled_(true) {
    
    log_file_.open(log_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "[AuditLogger] Warning: Could not open log file: " << log_path << std::endl;
    }
}

AuditLogger::~AuditLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void AuditLogger::log(uint32_t user_id, const std::string& username,
                      OperationType op, const std::string& target,
                      bool success, const std::string& details,
                      const std::string& error, const std::string& client_ip) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    AuditLogEntry entry;
    entry.log_id = next_log_id_++;
    entry.timestamp = time(nullptr);
    entry.user_id = user_id;
    entry.username = username;
    entry.client_ip = client_ip;
    entry.operation = op;
    entry.target = target;
    entry.details = details;
    entry.success = success;
    entry.error_message = error;
    
    write_entry(entry);
    
    // 保存到内存缓存
    recent_logs_.push_back(entry);
    if (recent_logs_.size() > MAX_RECENT_LOGS) {
        recent_logs_.erase(recent_logs_.begin());
    }
}

void AuditLogger::log_login(uint32_t user_id, const std::string& username,
                            bool success, const std::string& ip) {
    log(user_id, username, OperationType::USER_LOGIN, "login", success,
        success ? "User logged in" : "Login attempt failed", "", ip);
}

void AuditLogger::log_paper_submit(uint32_t user_id, const std::string& username,
                                   uint32_t paper_id, const std::string& title) {
    log(user_id, username, OperationType::PAPER_SUBMIT, 
        "paper:" + std::to_string(paper_id), true,
        "Submitted paper: " + title);
}

void AuditLogger::log_review_submit(uint32_t user_id, const std::string& username,
                                    uint32_t paper_id, int score) {
    log(user_id, username, OperationType::REVIEW_SUBMIT,
        "paper:" + std::to_string(paper_id), true,
        "Submitted review with score: " + std::to_string(score));
}

void AuditLogger::log_decision(uint32_t user_id, const std::string& username,
                               uint32_t paper_id, const std::string& decision) {
    log(user_id, username, OperationType::DECISION_MAKE,
        "paper:" + std::to_string(paper_id), true,
        "Made decision: " + decision);
}

std::vector<AuditLogEntry> AuditLogger::get_recent_logs(size_t count) const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (count >= recent_logs_.size()) {
        return recent_logs_;
    }
    
    return std::vector<AuditLogEntry>(
        recent_logs_.end() - count, recent_logs_.end());
}

std::vector<AuditLogEntry> AuditLogger::get_logs_by_user(uint32_t user_id, size_t count) const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::vector<AuditLogEntry> result;
    for (auto it = recent_logs_.rbegin(); it != recent_logs_.rend() && result.size() < count; ++it) {
        if (it->user_id == user_id) {
            result.push_back(*it);
        }
    }
    return result;
}

std::vector<AuditLogEntry> AuditLogger::get_logs_by_operation(OperationType op, size_t count) const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::vector<AuditLogEntry> result;
    for (auto it = recent_logs_.rbegin(); it != recent_logs_.rend() && result.size() < count; ++it) {
        if (it->operation == op) {
            result.push_back(*it);
        }
    }
    return result;
}

std::vector<AuditLogEntry> AuditLogger::get_logs_in_timerange(time_t start, time_t end) const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::vector<AuditLogEntry> result;
    for (const auto& entry : recent_logs_) {
        if (entry.timestamp >= start && entry.timestamp <= end) {
            result.push_back(entry);
        }
    }
    return result;
}

AuditLogger::LogStatistics AuditLogger::get_statistics() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    LogStatistics stats;
    stats.total_logs = recent_logs_.size();
    stats.login_count = 0;
    stats.failed_logins = 0;
    stats.paper_submissions = 0;
    stats.reviews_submitted = 0;
    stats.decisions_made = 0;
    
    for (const auto& entry : recent_logs_) {
        stats.operation_counts[entry.operation]++;
        
        if (entry.operation == OperationType::USER_LOGIN) {
            stats.login_count++;
            if (!entry.success) stats.failed_logins++;
        } else if (entry.operation == OperationType::PAPER_SUBMIT) {
            stats.paper_submissions++;
        } else if (entry.operation == OperationType::REVIEW_SUBMIT) {
            stats.reviews_submitted++;
        } else if (entry.operation == OperationType::DECISION_MAKE) {
            stats.decisions_made++;
        }
    }
    
    return stats;
}

bool AuditLogger::export_to_file(const std::string& path, time_t start, time_t end) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    out << "[\n";
    bool first = true;
    for (const auto& entry : recent_logs_) {
        if ((start == 0 || entry.timestamp >= start) &&
            (end == 0 || entry.timestamp <= end)) {
            if (!first) out << ",\n";
            out << "  " << entry.to_json();
            first = false;
        }
    }
    out << "\n]\n";
    
    return true;
}

void AuditLogger::write_entry(const AuditLogEntry& entry) {
    if (log_file_.is_open()) {
        log_file_ << entry.to_string() << std::endl;
        log_file_.flush();
    }
}

std::string AuditLogger::operation_to_string(OperationType op) const {
    switch (op) {
        case OperationType::USER_LOGIN: return "USER_LOGIN";
        case OperationType::USER_LOGOUT: return "USER_LOGOUT";
        case OperationType::USER_CREATE: return "USER_CREATE";
        case OperationType::USER_DELETE: return "USER_DELETE";
        case OperationType::PAPER_SUBMIT: return "PAPER_SUBMIT";
        case OperationType::PAPER_REVISE: return "PAPER_REVISE";
        case OperationType::PAPER_DOWNLOAD: return "PAPER_DOWNLOAD";
        case OperationType::PAPER_DELETE: return "PAPER_DELETE";
        case OperationType::REVIEWER_ASSIGN: return "REVIEWER_ASSIGN";
        case OperationType::REVIEW_SUBMIT: return "REVIEW_SUBMIT";
        case OperationType::DECISION_MAKE: return "DECISION_MAKE";
        case OperationType::SYSTEM_BACKUP: return "SYSTEM_BACKUP";
        case OperationType::SYSTEM_RESTORE: return "SYSTEM_RESTORE";
        default: return "UNKNOWN";
    }
}

// ==================== FSJournal 实现 ====================
FSJournal::FSJournal(const std::string& journal_path)
    : journal_path_(journal_path), next_seq_(1), 
      recovery_mode_(false), in_transaction_(false) {
    
    journal_file_.open(journal_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!journal_file_.is_open()) {
        // 创建新文件
        journal_file_.open(journal_path, std::ios::out | std::ios::binary);
        journal_file_.close();
        journal_file_.open(journal_path, std::ios::in | std::ios::out | std::ios::binary);
    }
}

FSJournal::~FSJournal() {
    if (in_transaction_) {
        rollback_transaction();
    }
    if (journal_file_.is_open()) {
        journal_file_.close();
    }
}

bool FSJournal::begin_transaction() {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (in_transaction_) {
        return false;  // 已经在事务中
    }
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::BEGIN_TRANSACTION;
    entry.timestamp = time(nullptr);
    entry.committed = false;
    
    if (!write_log_entry(entry)) {
        return false;
    }
    
    current_transaction_.clear();
    current_transaction_.push_back(entry);
    in_transaction_ = true;
    
    return true;
}

bool FSJournal::commit_transaction() {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) {
        return false;
    }
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::END_TRANSACTION;
    entry.timestamp = time(nullptr);
    entry.committed = true;
    
    if (!write_log_entry(entry)) {
        return false;
    }
    
    current_transaction_.clear();
    in_transaction_ = false;
    
    return true;
}

bool FSJournal::rollback_transaction() {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) {
        return false;
    }
    
    // 回滚：需要应用所有操作的逆操作
    // 这里简化处理，只清除事务
    current_transaction_.clear();
    in_transaction_ = false;
    
    return true;
}

void FSJournal::log_write_block(uint64_t block_num, 
                                const std::vector<uint8_t>& old_data,
                                const std::vector<uint8_t>& new_data) {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) return;
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::WRITE_BLOCK;
    entry.block_number = block_num;
    entry.old_data = old_data;
    entry.new_data = new_data;
    entry.timestamp = time(nullptr);
    
    write_log_entry(entry);
    current_transaction_.push_back(entry);
}

void FSJournal::log_allocate_block(uint64_t block_num) {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) return;
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::ALLOCATE_BLOCK;
    entry.block_number = block_num;
    entry.timestamp = time(nullptr);
    
    write_log_entry(entry);
    current_transaction_.push_back(entry);
}

void FSJournal::log_free_block(uint64_t block_num) {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) return;
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::FREE_BLOCK;
    entry.block_number = block_num;
    entry.timestamp = time(nullptr);
    
    write_log_entry(entry);
    current_transaction_.push_back(entry);
}

void FSJournal::log_update_inode(uint32_t inode_num,
                                 const std::vector<uint8_t>& old_data,
                                 const std::vector<uint8_t>& new_data) {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!in_transaction_) return;
    
    FSLogEntry entry;
    entry.log_seq = next_seq_++;
    entry.type = FSLogType::UPDATE_INODE;
    entry.inode_number = inode_num;
    entry.old_data = old_data;
    entry.new_data = new_data;
    entry.timestamp = time(nullptr);
    
    write_log_entry(entry);
    current_transaction_.push_back(entry);
}

bool FSJournal::needs_recovery() const {
    // 检查是否有未提交的事务
    std::vector<FSLogEntry> entries;
    // 读取日志检查
    return false;  // 简化实现
}

bool FSJournal::recover(std::function<bool(const FSLogEntry&)> apply_callback) {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    std::vector<FSLogEntry> entries;
    if (!read_log_entries(entries)) {
        return false;
    }
    
    // 找到所有已提交的事务并重放
    bool in_txn = false;
    std::vector<FSLogEntry> current_txn;
    
    for (const auto& entry : entries) {
        if (entry.type == FSLogType::BEGIN_TRANSACTION) {
            in_txn = true;
            current_txn.clear();
        } else if (entry.type == FSLogType::END_TRANSACTION && entry.committed) {
            // 重放这个事务
            for (const auto& txn_entry : current_txn) {
                apply_callback(txn_entry);
            }
            in_txn = false;
        } else if (in_txn) {
            current_txn.push_back(entry);
        }
    }
    
    truncate_journal();
    return true;
}

bool FSJournal::checkpoint() {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (in_transaction_) {
        return false;  // 不能在事务中创建检查点
    }
    
    truncate_journal();
    return true;
}

uint64_t FSJournal::get_journal_size() const {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    
    if (!journal_file_.is_open()) return 0;
    
    auto current_pos = journal_file_.tellg();
    journal_file_.seekg(0, std::ios::end);
    auto size = journal_file_.tellg();
    journal_file_.seekg(current_pos);
    
    return static_cast<uint64_t>(size);
}

uint64_t FSJournal::get_pending_entries() const {
    std::lock_guard<std::mutex> lock(journal_mutex_);
    return current_transaction_.size();
}

bool FSJournal::write_log_entry(const FSLogEntry& entry) {
    if (!journal_file_.is_open()) return false;
    
    journal_file_.seekp(0, std::ios::end);
    
    // 写入序列号
    journal_file_.write(reinterpret_cast<const char*>(&entry.log_seq), sizeof(entry.log_seq));
    // 写入类型
    journal_file_.write(reinterpret_cast<const char*>(&entry.type), sizeof(entry.type));
    // 写入块号
    journal_file_.write(reinterpret_cast<const char*>(&entry.block_number), sizeof(entry.block_number));
    // 写入inode号
    journal_file_.write(reinterpret_cast<const char*>(&entry.inode_number), sizeof(entry.inode_number));
    // 写入时间戳
    journal_file_.write(reinterpret_cast<const char*>(&entry.timestamp), sizeof(entry.timestamp));
    // 写入committed标志
    journal_file_.write(reinterpret_cast<const char*>(&entry.committed), sizeof(entry.committed));
    
    journal_file_.flush();
    
    return !journal_file_.fail();
}

bool FSJournal::read_log_entries(std::vector<FSLogEntry>& entries) {
    if (!journal_file_.is_open()) return false;
    
    journal_file_.seekg(0, std::ios::beg);
    entries.clear();
    
    while (journal_file_.good()) {
        FSLogEntry entry;
        
        journal_file_.read(reinterpret_cast<char*>(&entry.log_seq), sizeof(entry.log_seq));
        if (journal_file_.eof()) break;
        
        journal_file_.read(reinterpret_cast<char*>(&entry.type), sizeof(entry.type));
        journal_file_.read(reinterpret_cast<char*>(&entry.block_number), sizeof(entry.block_number));
        journal_file_.read(reinterpret_cast<char*>(&entry.inode_number), sizeof(entry.inode_number));
        journal_file_.read(reinterpret_cast<char*>(&entry.timestamp), sizeof(entry.timestamp));
        journal_file_.read(reinterpret_cast<char*>(&entry.committed), sizeof(entry.committed));
        
        if (journal_file_.good()) {
            entries.push_back(entry);
        }
    }
    
    return true;
}

void FSJournal::truncate_journal() {
    if (journal_file_.is_open()) {
        journal_file_.close();
    }
    
    // 清空文件
    std::ofstream clear_file(journal_path_, std::ios::trunc | std::ios::binary);
    clear_file.close();
    
    // 重新打开
    journal_file_.open(journal_path_, std::ios::in | std::ios::out | std::ios::binary);
    next_seq_ = 1;
}

// ==================== LogUtils 实现 ====================
namespace LogUtils {
    std::string level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    std::string time_to_string(time_t t) {
        char buffer[64];
        struct tm* tm_info = localtime(&t);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        return std::string(buffer);
    }
    
    std::string format_log_entry(const AuditLogEntry& entry) {
        std::stringstream ss;
        ss << "[" << time_to_string(entry.timestamp) << "] ";
        ss << std::setw(12) << entry.username << " | ";
        ss << std::setw(15) << static_cast<int>(entry.operation) << " | ";
        ss << entry.target;
        if (!entry.success) {
            ss << " [FAILED: " << entry.error_message << "]";
        }
        return ss.str();
    }
    
    std::string colorize(const std::string& text, const std::string& color) {
        return color + text + COLOR_RESET;
    }
}
