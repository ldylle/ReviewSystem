#ifndef REVIEW_WORKFLOW_H
#define REVIEW_WORKFLOW_H

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <memory>
#include <functional>

// ==================== 审稿轮次 ====================
enum class ReviewRound {
    FIRST_ROUND = 1,
    SECOND_ROUND = 2,
    THIRD_ROUND = 3,
    FINAL_ROUND = 4
};

// ==================== 截止日期管理 ====================
struct Deadline {
    uint32_t paper_id;
    ReviewRound round;
    time_t submission_deadline;     // 审稿提交截止
    time_t revision_deadline;       // 作者修改截止
    time_t decision_deadline;       // 编辑决定截止
    bool is_extended;
    std::string extension_reason;
    
    Deadline() : paper_id(0), round(ReviewRound::FIRST_ROUND),
                 submission_deadline(0), revision_deadline(0),
                 decision_deadline(0), is_extended(false) {}
    
    bool is_overdue() const {
        return time(nullptr) > submission_deadline;
    }
    
    int days_remaining() const {
        time_t now = time(nullptr);
        int diff = (submission_deadline - now) / 86400;
        return diff;
    }
};

// ==================== 审稿任务 ====================
struct ReviewTask {
    uint32_t task_id;
    uint32_t paper_id;
    uint32_t reviewer_id;
    ReviewRound round;
    time_t assigned_time;
    time_t deadline;
    time_t completed_time;
    bool is_completed;
    bool is_declined;
    std::string decline_reason;
    int reminder_count;
    time_t last_reminder;
    
    ReviewTask() : task_id(0), paper_id(0), reviewer_id(0),
                   round(ReviewRound::FIRST_ROUND), assigned_time(0),
                   deadline(0), completed_time(0), is_completed(false),
                   is_declined(false), reminder_count(0), last_reminder(0) {}
    
    bool is_overdue() const {
        return !is_completed && time(nullptr) > deadline;
    }
};

// ==================== 多轮审稿记录 ====================
struct RoundReview {
    uint32_t paper_id;
    ReviewRound round;
    std::vector<uint32_t> reviewer_ids;
    std::map<uint32_t, int> scores;          // reviewer_id -> score
    std::map<uint32_t, std::string> comments; // reviewer_id -> comments
    std::map<uint32_t, int> recommendations;  // reviewer_id -> recommendation
    time_t start_time;
    time_t end_time;
    std::string round_decision;              // 本轮决定
    std::string editor_comments;
    
    RoundReview() : paper_id(0), round(ReviewRound::FIRST_ROUND),
                    start_time(0), end_time(0) {}
    
    double average_score() const {
        if (scores.empty()) return 0;
        double sum = 0;
        for (const auto& [id, score] : scores) sum += score;
        return sum / scores.size();
    }
    
    int completed_reviews() const {
        return scores.size();
    }
};

// ==================== 审稿流程配置 ====================
struct WorkflowConfig {
    int default_review_days;        // 默认审稿天数
    int default_revision_days;      // 默认修改天数
    int max_review_rounds;          // 最大审稿轮数
    int min_reviewers;              // 最少审稿人数
    int max_reviewers;              // 最多审稿人数
    int reminder_interval_days;     // 提醒间隔天数
    int max_reminders;              // 最大提醒次数
    bool auto_remind;               // 是否自动提醒
    bool auto_escalate;             // 是否自动升级(超时)
    
    WorkflowConfig() : default_review_days(14), default_revision_days(30),
                       max_review_rounds(3), min_reviewers(2), max_reviewers(5),
                       reminder_interval_days(3), max_reminders(3),
                       auto_remind(true), auto_escalate(false) {}
};

// ==================== 通知类型 ====================
enum class NotificationType {
    PAPER_ASSIGNED,             // 论文已分配
    REVIEW_REMINDER,            // 审稿提醒
    REVIEW_OVERDUE,             // 审稿超时
    REVIEW_SUBMITTED,           // 评审已提交
    ALL_REVIEWS_COMPLETE,       // 所有评审完成
    DECISION_MADE,              // 决定已作出
    REVISION_REQUIRED,          // 需要修改
    REVISION_SUBMITTED,         // 修改已提交
    PAPER_ACCEPTED,             // 论文接受
    PAPER_REJECTED              // 论文拒绝
};

// ==================== 通知 ====================
struct Notification {
    uint32_t notification_id;
    uint32_t recipient_id;
    NotificationType type;
    uint32_t paper_id;
    std::string title;
    std::string message;
    time_t created_time;
    bool is_read;
    bool is_sent;               // 是否已发送(用于邮件等)
    
    Notification() : notification_id(0), recipient_id(0),
                     type(NotificationType::PAPER_ASSIGNED), paper_id(0),
                     created_time(0), is_read(false), is_sent(false) {}
};

// ==================== 审稿流程管理器 ====================
class ReviewWorkflowManager {
private:
    WorkflowConfig config_;
    std::map<uint32_t, std::vector<RoundReview>> paper_rounds_;  // paper_id -> rounds
    std::map<uint32_t, ReviewTask> tasks_;                       // task_id -> task
    std::map<uint32_t, Deadline> deadlines_;                     // paper_id -> deadline
    std::vector<Notification> notifications_;
    
    uint32_t next_task_id_;
    uint32_t next_notification_id_;
    
    // 回调函数(用于触发实际动作)
    std::function<void(const Notification&)> notification_callback_;
    
public:
    ReviewWorkflowManager();
    
    // ===== 配置 =====
    void set_config(const WorkflowConfig& config) { config_ = config; }
    WorkflowConfig get_config() const { return config_; }
    
    // ===== 截止日期管理 =====
    void set_deadline(uint32_t paper_id, ReviewRound round, 
                     time_t review_deadline, time_t revision_deadline = 0);
    bool extend_deadline(uint32_t paper_id, int extra_days, const std::string& reason);
    Deadline get_deadline(uint32_t paper_id) const;
    std::vector<uint32_t> get_overdue_papers() const;
    std::vector<uint32_t> get_approaching_deadline_papers(int days = 3) const;
    
    // ===== 审稿任务管理 =====
    uint32_t create_task(uint32_t paper_id, uint32_t reviewer_id, ReviewRound round);
    bool complete_task(uint32_t task_id, int score, const std::string& comments, int recommendation);
    bool decline_task(uint32_t task_id, const std::string& reason);
    ReviewTask get_task(uint32_t task_id) const;
    std::vector<ReviewTask> get_tasks_by_reviewer(uint32_t reviewer_id) const;
    std::vector<ReviewTask> get_tasks_by_paper(uint32_t paper_id) const;
    std::vector<ReviewTask> get_overdue_tasks() const;
    
    // ===== 多轮审稿 =====
    bool start_round(uint32_t paper_id, ReviewRound round, 
                    const std::vector<uint32_t>& reviewer_ids);
    bool end_round(uint32_t paper_id, ReviewRound round, 
                  const std::string& decision, const std::string& editor_comments);
    RoundReview get_round(uint32_t paper_id, ReviewRound round) const;
    std::vector<RoundReview> get_all_rounds(uint32_t paper_id) const;
    ReviewRound get_current_round(uint32_t paper_id) const;
    
    // ===== 进度追踪 =====
    struct PaperProgress {
        uint32_t paper_id;
        ReviewRound current_round;
        int total_reviewers;
        int completed_reviews;
        double average_score;
        int days_in_review;
        int days_until_deadline;
        std::string status;
        std::vector<std::string> pending_reviewers;
    };
    PaperProgress get_progress(uint32_t paper_id) const;
    std::string render_progress_chart(uint32_t paper_id) const;
    
    // ===== 通知系统 =====
    void set_notification_callback(std::function<void(const Notification&)> callback);
    void send_notification(uint32_t recipient_id, NotificationType type,
                          uint32_t paper_id, const std::string& message);
    std::vector<Notification> get_notifications(uint32_t user_id, bool unread_only = false) const;
    void mark_notification_read(uint32_t notification_id);
    
    // ===== 自动处理 =====
    void process_reminders();       // 发送提醒
    void process_escalations();     // 处理超时升级
    void daily_maintenance();       // 每日维护任务
    
    // ===== 统计 =====
    struct WorkflowStatistics {
        int active_papers;
        int total_tasks;
        int completed_tasks;
        int overdue_tasks;
        double average_review_time;
        double on_time_rate;
        std::map<ReviewRound, int> papers_per_round;
    };
    WorkflowStatistics get_statistics() const;
    
private:
    void notify(uint32_t recipient_id, NotificationType type,
               uint32_t paper_id, const std::string& message);
    std::string notification_type_to_string(NotificationType type) const;
};

// ==================== 审稿评分模板 ====================
struct ReviewTemplate {
    std::string template_name;
    std::vector<std::string> criteria;
    std::map<std::string, int> weights;  // 各项权重
    std::vector<std::string> required_sections;
    std::string guidelines;
    
    ReviewTemplate() = default;
    
    // 预定义模板
    static ReviewTemplate standard_template();
    static ReviewTemplate double_blind_template();
    static ReviewTemplate technical_paper_template();
};

// ==================== 评审表单 ====================
struct ReviewForm {
    uint32_t paper_id;
    uint32_t reviewer_id;
    
    // 各维度评分 (1-10)
    int originality;
    int technical_quality;
    int clarity;
    int significance;
    int relevance;
    
    // 详细评价
    std::string strengths;
    std::string weaknesses;
    std::string detailed_comments;
    std::string confidential_comments;
    
    // 总体评价
    int overall_score;
    int recommendation;  // 1=Accept, 2=Minor, 3=Major, 4=Reject
    int confidence;      // 1-5
    
    ReviewForm() : paper_id(0), reviewer_id(0), originality(0),
                   technical_quality(0), clarity(0), significance(0),
                   relevance(0), overall_score(0), recommendation(0), confidence(0) {}
    
    double weighted_score() const {
        return 0.25 * originality + 0.30 * technical_quality +
               0.15 * clarity + 0.20 * significance + 0.10 * relevance;
    }
    
    bool is_complete() const {
        return originality > 0 && technical_quality > 0 && clarity > 0 &&
               significance > 0 && overall_score > 0 && recommendation > 0;
    }
};

// ==================== 辅助函数 ====================
namespace WorkflowUtils {
    std::string round_to_string(ReviewRound round);
    ReviewRound string_to_round(const std::string& str);
    std::string recommendation_to_string(int rec);
    std::string format_deadline(time_t deadline);
    std::string format_duration(int days);
    
    // 生成审稿报告
    std::string generate_review_summary(const std::vector<ReviewForm>& reviews);
    
    // 计算建议决定
    int suggest_decision(const std::vector<ReviewForm>& reviews);
}

#endif // REVIEW_WORKFLOW_H
