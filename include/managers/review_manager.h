#ifndef REVIEW_MANAGER_H
#define REVIEW_MANAGER_H

#include "filesystem.h"
#include "auth_manager.h"
#include "protocol.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <ctime>

// ==================== 论文信息 ====================
struct Paper {
    uint32_t paper_id;
    std::string title;
    std::string abstract;
    std::vector<uint32_t> author_ids;
    std::vector<std::string> author_names;
    std::vector<std::string> keywords;
    std::string research_area;
    PaperStatus status;
    time_t submit_time;
    time_t last_update;
    uint32_t version;                           // 当前版本号
    std::string current_version_path;           // 当前版本文件路径
    std::vector<uint32_t> reviewer_ids;        // 分配的审稿人
    DecisionType final_decision;
    std::string decision_comment;
    uint32_t editor_id;         
    
    std::vector<std::string> authors;  // 作者名列表 (兼容 author_names)
    uint32_t submitter_id;             // 提交者ID// 负责编辑
    
    Paper() : paper_id(0), status(PaperStatus::SUBMITTED),
              submit_time(0), last_update(0), version(1),
              final_decision(DecisionType::REJECT), editor_id(0) {}
};

// ==================== 论文版本 ====================
struct PaperVersion {
    uint32_t paper_id;
    uint32_t version;
    std::string file_path;
    time_t upload_time;
    std::string change_description;
    std::vector<uint8_t> content;               // 论文内容
    
    PaperVersion() : paper_id(0), version(0), upload_time(0) {}
};

// ==================== 审稿意见 ====================
struct Review {
    uint32_t review_id;
    uint32_t paper_id;
    uint32_t reviewer_id;
    std::string reviewer_name;
    uint32_t paper_version;                     // 审稿的论文版本
    int32_t score;                              // 评分 1-10
    std::string comments;                       // 审稿意见
    std::string confidential_comments;          // 保密意见(仅编辑可见)
    DecisionType recommendation;                 // 推荐决策
    time_t submit_time;
    bool is_completed;
    
    Review() : review_id(0), paper_id(0), reviewer_id(0),
               paper_version(0), score(0),
               recommendation(DecisionType::REJECT),
               submit_time(0), is_completed(false) {}
};

// ==================== 审稿决策 ====================
struct EditorialDecision {
    uint32_t decision_id;
    uint32_t paper_id;
    uint32_t editor_id;
    DecisionType decision;
    std::string comments;
    std::vector<uint32_t> review_ids;           // 参考的审稿意见
    time_t decision_time;
    
    EditorialDecision() : decision_id(0), paper_id(0), editor_id(0),
                         decision(DecisionType::REJECT), decision_time(0) {}
};

// ==================== 审稿管理器 ====================
class ReviewManager {
private:
    std::shared_ptr<FileSystem> fs_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<COIDetector> coi_detector_;
    std::shared_ptr<ReviewerMatcher> reviewer_matcher_;
    
    std::map<uint32_t, Paper> papers_;          // 论文ID -> 论文
    std::map<uint32_t, std::vector<Review>> paper_reviews_;  // 论文ID -> 审稿意见列表
    std::map<uint32_t, std::vector<PaperVersion>> paper_versions_;  // 论文ID -> 版本列表
    std::map<uint32_t, EditorialDecision> decisions_;  // 论文ID -> 决策
    
    mutable std::mutex review_mutex_;
    uint32_t next_paper_id_;
    uint32_t next_review_id_;
    uint32_t next_decision_id_;
    
    // 配置参数
    uint32_t min_reviewers_per_paper_;          // 每篇论文最少审稿人数
    uint32_t max_reviewers_per_paper_;          // 每篇论文最多审稿人数
    uint32_t max_papers_per_reviewer_;          // 每个审稿人最多论文数
    
public:

    // 获取论文的所有评审
    std::vector<Review> get_paper_reviews(uint32_t paper_id);
    
    // 获取审稿人分配的论文
    std::vector<Paper> get_reviewer_papers(uint32_t reviewer_id);
    ReviewManager(std::shared_ptr<FileSystem> fs,
                 std::shared_ptr<AuthManager> auth_manager);
    ~ReviewManager();
    
    // ===== 初始化 =====
    bool initialize();
    
    // ===== 论文提交 =====
    int submit_paper(uint32_t author_id, const std::string& title,
                    const std::string& abstract,
                    const std::vector<std::string>& authors,
                    const std::vector<std::string>& keywords,
                    const std::string& research_area,
                    const std::vector<uint8_t>& paper_content);
    
    // ===== 论文修订 =====
    bool revise_paper(uint32_t paper_id, uint32_t author_id,
                     const std::vector<uint8_t>& revised_content,
                     const std::string& change_description = "");
    
    // ===== 论文下载 =====
    bool download_paper(uint32_t paper_id, uint32_t user_id,
                       std::vector<uint8_t>& content);
    
    // ===== 论文删除 =====
    bool delete_paper(uint32_t paper_id, uint32_t user_id);
    
    // ===== 审稿人分配 =====
    bool assign_reviewers(uint32_t paper_id, uint32_t editor_id,
                         const std::vector<uint32_t>& reviewer_ids);
    
    // 自动分配审稿人
    std::vector<uint32_t> auto_assign_reviewers(uint32_t paper_id,
                                               uint32_t editor_id,
                                               size_t num_reviewers = 3);
    
    // ===== 审稿意见提交 =====
    int submit_review(uint32_t paper_id, uint32_t reviewer_id,
                     int32_t score, const std::string& comments,
                     const std::string& confidential_comments,
                     DecisionType recommendation);
    
    // ===== 审稿意见下载 =====
    std::vector<Review> get_reviews(uint32_t paper_id, uint32_t user_id);
    
    // ===== 编辑决策 =====
    bool make_decision(uint32_t paper_id, uint32_t editor_id,
                      DecisionType decision, const std::string& comments);
    
    // ===== 论文查询 =====
    bool get_paper(uint32_t paper_id, Paper& paper);
    std::vector<Paper> get_papers_by_author(uint32_t author_id);
    std::vector<Paper> get_papers_by_reviewer(uint32_t reviewer_id);
    std::vector<Paper> get_papers_by_status(PaperStatus status);
    std::vector<Paper> get_all_papers();
    
    // ===== 论文状态 =====
    bool get_paper_status(uint32_t paper_id, uint32_t user_id,
                         Paper& paper, std::vector<Review>& reviews);
    
    // ===== 版本管理 =====
    std::vector<PaperVersion> get_paper_versions(uint32_t paper_id);
    bool get_paper_version(uint32_t paper_id, uint32_t version,
                          PaperVersion& paper_version);
    
    // ===== 统计信息 =====
    size_t get_total_papers() const;
    size_t get_papers_by_status_count(PaperStatus status) const;
    std::map<PaperStatus, size_t> get_status_distribution() const;
    
    // ===== 审稿进度 =====
    struct ReviewProgress {
        uint32_t paper_id;
        uint32_t total_reviewers;
        uint32_t completed_reviews;
        double progress_percentage;
        PaperStatus status;
    };
    ReviewProgress get_review_progress(uint32_t paper_id);
    
    // ===== 审稿人工作量 =====
    struct ReviewerWorkload {
        uint32_t reviewer_id;
        uint32_t pending_reviews;
        uint32_t completed_reviews;
        double average_score;
    };
    ReviewerWorkload get_reviewer_workload(uint32_t reviewer_id);
    
    // ===== 持久化 =====
    bool save_data();
    bool load_data();
    
private:
    // ===== 文件路径管理 =====
    std::string get_paper_path(uint32_t paper_id, uint32_t version = 0);
    std::string get_review_path(uint32_t review_id);
    std::string get_data_dir() const { return "/papers"; }
    std::string get_db_path() const { return "/papers.db"; }
    
    // ===== 权限检查 =====
    bool can_access_paper(uint32_t paper_id, uint32_t user_id, UserRole role);
    bool can_review_paper(uint32_t paper_id, uint32_t reviewer_id);
    bool can_make_decision(uint32_t paper_id, uint32_t editor_id);
    
    // ===== 状态转换 =====
    bool update_paper_status(uint32_t paper_id, PaperStatus new_status);
    bool validate_status_transition(PaperStatus from, PaperStatus to);
    
    // ===== 通知系统(简化实现) =====
    void notify_reviewers(uint32_t paper_id, const std::vector<uint32_t>& reviewer_ids);
    void notify_author(uint32_t paper_id, const std::string& message);
    void notify_editor(uint32_t paper_id, const std::string& message);
};

// ==================== 统计分析器 ====================
class ReviewStatistics {
private:
    std::shared_ptr<ReviewManager> review_manager_;
    
public:
    explicit ReviewStatistics(std::shared_ptr<ReviewManager> review_manager);
    
    // 论文接收率
    double get_acceptance_rate();
    
    // 平均审稿时间
    double get_average_review_time();
    
    // 审稿人活跃度
    std::map<uint32_t, uint32_t> get_reviewer_activity();
    
    // 研究领域分布
    std::map<std::string, uint32_t> get_research_area_distribution();
    
    // 论文状态分布
    std::map<PaperStatus, uint32_t> get_status_distribution();
    
    // 生成统计报告
    std::string generate_report();
};

// ==================== 工具函数 ====================
namespace ReviewUtils {
    // 状态转字符串
    std::string status_to_string(PaperStatus status);
    
    // 决策转字符串
    std::string decision_to_string(DecisionType decision);
    
    // 生成论文ID
    std::string generate_paper_id(uint32_t id);
    
    // 验证论文格式
    bool validate_paper_format(const std::vector<uint8_t>& content);
    
    // 提取论文元数据
    bool extract_paper_metadata(const std::vector<uint8_t>& content,
                               std::string& title, std::string& abstract);
    
    // 计算论文相似度
    double calculate_similarity(uint32_t paper1_id, uint32_t paper2_id);
    
    // 关键词匹配
    double keyword_match_score(const std::vector<std::string>& keywords1,
                              const std::vector<std::string>& keywords2);
}

#endif // REVIEW_MANAGER_H
