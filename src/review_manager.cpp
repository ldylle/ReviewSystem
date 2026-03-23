#include "review_manager.h"
#include <algorithm>
#include <sstream>
#include <iostream>

// ==================== ReviewManager实现 ====================
ReviewManager::ReviewManager(std::shared_ptr<FileSystem> fs,
                             std::shared_ptr<AuthManager> auth_manager)
    : fs_(fs), auth_manager_(auth_manager),
      next_paper_id_(1001), next_review_id_(2001), next_decision_id_(3001),
      min_reviewers_per_paper_(3), max_reviewers_per_paper_(5),
      max_papers_per_reviewer_(10) {
    
    coi_detector_ = std::make_shared<COIDetector>(auth_manager);
    reviewer_matcher_ = std::make_shared<ReviewerMatcher>(auth_manager, coi_detector_);
}

ReviewManager::~ReviewManager() {
    save_data();
}

bool ReviewManager::initialize() {
    // 确保根数据目录存在
    if (!fs_->exists(get_data_dir())) {
        if (fs_->create_directory(get_data_dir()) < 0) {
             std::cerr << "[ReviewManager] Failed to create data dir: " << get_data_dir() << std::endl;
             return false;
        }
    }
    
    return load_data();
}

int ReviewManager::submit_paper(uint32_t author_id, const std::string& title,
                                const std::string& abstract,
                                const std::vector<std::string>& authors,
                                const std::vector<std::string>& keywords,
                                const std::string& research_area,
                                const std::vector<uint8_t>& paper_content) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    // 创建论文记录
    Paper paper;
    paper.paper_id = next_paper_id_++;
    paper.title = title;
    paper.abstract = abstract;
    paper.author_ids.push_back(author_id);
    paper.author_names = authors; // 兼容性字段
    paper.authors = authors;      // 新字段
    paper.keywords = keywords;
    paper.research_area = research_area;
    paper.status = PaperStatus::SUBMITTED;
    paper.submit_time = time(nullptr);
    paper.last_update = paper.submit_time;
    paper.version = 1;
    paper.submitter_id = author_id;
    
    std::string paper_path = get_paper_path(paper.paper_id, 1);
    paper.current_version_path = paper_path;
    
    // 1. 确保根数据目录存在 (防止 initialize 未被正确调用或目录被删)
    if (!fs_->exists(get_data_dir())) {
        if (fs_->create_directory(get_data_dir()) < 0) {
            std::cerr << "[ReviewManager] Error: Root data dir missing and cannot create: " << get_data_dir() << std::endl;
            return -1;
        }
    }
    
    // 2. 创建论文专属目录 (例如 /papers/1001)
    std::string paper_dir = get_data_dir() + "/" + std::to_string(paper.paper_id);
    if (!fs_->exists(paper_dir)) {
        if (fs_->create_directory(paper_dir) < 0) {
            std::cerr << "[ReviewManager] Error: Failed to create paper directory: " << paper_dir << std::endl;
            return -1;
        }
    }
    
    // 3. 创建空文件 (文件系统要求写入前文件必须存在)
    if (fs_->create_file(paper_path) < 0) {
        // 如果创建失败，检查是否文件已存在（极罕见情况，但需处理）
        if (!fs_->exists(paper_path)) {
            std::cerr << "[ReviewManager] Error: Failed to create paper file: " << paper_path << std::endl;
            return -1;
        }
    }

    // 4. 写入论文内容
    ssize_t written = fs_->write_file(paper_path, paper_content);
    if (written < 0) {
        std::cerr << "[ReviewManager] Error: Failed to write content to " << paper_path << std::endl;
        return -1;
    }
    
    // 保存版本信息
    PaperVersion version;
    version.paper_id = paper.paper_id;
    version.version = 1;
    version.file_path = paper_path;
    version.upload_time = paper.submit_time;
    version.content = paper_content;
    
    papers_[paper.paper_id] = paper;
    paper_versions_[paper.paper_id].push_back(version);
    
    if (!save_data()) {
        std::cerr << "[ReviewManager] Warning: Failed to save metadata database" << std::endl;
    }
    
    std::cout << "[ReviewManager] Paper submitted successfully: " << paper.paper_id << std::endl;
    return paper.paper_id;
}

bool ReviewManager::revise_paper(uint32_t paper_id, uint32_t author_id,
                                 const std::vector<uint8_t>& revised_content,
                                 const std::string& change_description) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return false;
    }
    
    Paper& paper = it->second;
    
    // 检查是否是作者 (author_ids 是 vector)
    bool is_author = false;
    for (uint32_t id : paper.author_ids) {
        if (id == author_id) {
            is_author = true;
            break;
        }
    }
    if (!is_author) return false;
    
    // 创建新版本
    paper.version++;
    std::string new_path = get_paper_path(paper_id, paper.version);
    
    // 确保文件创建成功
    if (fs_->create_file(new_path) < 0 && !fs_->exists(new_path)) {
        paper.version--;
        return false;
    }
    
    if (fs_->write_file(new_path, revised_content) < 0) {
        paper.version--;
        return false;
    }
    
    paper.current_version_path = new_path;
    paper.last_update = time(nullptr);
    paper.status = PaperStatus::REVISED;
    
    // 保存版本信息
    PaperVersion version;
    version.paper_id = paper_id;
    version.version = paper.version;
    version.file_path = new_path;
    version.upload_time = paper.last_update;
    version.change_description = change_description;
    version.content = revised_content;
    
    paper_versions_[paper_id].push_back(version);
    
    save_data();
    
    return true;
}

bool ReviewManager::download_paper(uint32_t paper_id, uint32_t user_id,
                                   std::vector<uint8_t>& content) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return false;
    }
    
    const Paper& paper = it->second;
    
    // 读取论文内容
    return fs_->read_file(paper.current_version_path, content) >= 0;
}

bool ReviewManager::assign_reviewers(uint32_t paper_id, uint32_t editor_id,
                                     const std::vector<uint32_t>& reviewer_ids) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return false;
    }
    
    Paper& paper = it->second;
    paper.reviewer_ids = reviewer_ids;
    paper.editor_id = editor_id;
    paper.status = PaperStatus::UNDER_REVIEW;
    paper.last_update = time(nullptr);
    
    save_data();
    
    return true;
}

std::vector<uint32_t> ReviewManager::auto_assign_reviewers(uint32_t paper_id,
                                                           uint32_t editor_id,
                                                           size_t num_reviewers) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return {};
    }
    
    const Paper& paper = it->second;
    uint32_t first_author_id = paper.author_ids.empty() ? 0 : paper.author_ids[0];
    
    // 使用匹配器查找最佳审稿人
    auto matches = reviewer_matcher_->find_best_reviewers(
        paper.keywords,
        paper.research_area,
        first_author_id,
        num_reviewers,
        0.0
    );
    
    std::vector<uint32_t> reviewer_ids;
    for (const auto& match : matches) {
        reviewer_ids.push_back(match.reviewer_id);
    }
    
    // 只有找到审稿人才分配，但无论如何都返回找到的列表
    // 这里为了不破坏现有逻辑，如果找到足够的人或至少有建议，可以不自动分配状态，而是让外部调用 assign
    // 但原逻辑是直接分配
    
    return reviewer_ids;
}

int ReviewManager::submit_review(uint32_t paper_id, uint32_t reviewer_id,
                                 int32_t score, const std::string& comments,
                                 const std::string& confidential_comments,
                                 DecisionType recommendation) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return -1;
    }
    
    const Paper& paper = it->second;
    
    // 创建审稿意见
    Review review;
    review.review_id = next_review_id_++;
    review.paper_id = paper_id;
    review.reviewer_id = reviewer_id;
    review.paper_version = paper.version;
    review.score = score;
    review.comments = comments;
    review.confidential_comments = confidential_comments;
    review.recommendation = recommendation;
    review.submit_time = time(nullptr);
    review.is_completed = true;
    
    paper_reviews_[paper_id].push_back(review);
    
    save_data();
    
    return review.review_id;
}

std::vector<Review> ReviewManager::get_reviews(uint32_t paper_id, uint32_t user_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = paper_reviews_.find(paper_id);
    if (it == paper_reviews_.end()) {
        return {};
    }
    
    return it->second;
}

bool ReviewManager::make_decision(uint32_t paper_id, uint32_t editor_id,
                                  DecisionType decision, const std::string& comments) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return false;
    }
    
    Paper& paper = it->second;
    
    // 创建决策记录
    EditorialDecision ed;
    ed.decision_id = next_decision_id_++;
    ed.paper_id = paper_id;
    ed.editor_id = editor_id;
    ed.decision = decision;
    ed.comments = comments;
    ed.decision_time = time(nullptr);
    
    // 更新论文状态
    paper.final_decision = decision;
    paper.decision_comment = comments;
    paper.last_update = ed.decision_time;
    
    switch (decision) {
        case DecisionType::ACCEPT:
            paper.status = PaperStatus::ACCEPTED;
            break;
        case DecisionType::REJECT:
            paper.status = PaperStatus::REJECTED;
            break;
        case DecisionType::MAJOR_REVISION:
        case DecisionType::MINOR_REVISION:
            paper.status = PaperStatus::REVISION_REQUIRED;
            break;
    }
    
    decisions_[paper_id] = ed;
    
    save_data();
    
    return true;
}

bool ReviewManager::get_paper(uint32_t paper_id, Paper& paper) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = papers_.find(paper_id);
    if (it == papers_.end()) {
        return false;
    }
    
    paper = it->second;
    return true;
}

std::vector<Paper> ReviewManager::get_papers_by_author(uint32_t author_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        // 检查 author_ids
        for (uint32_t id : paper.author_ids) {
            if (id == author_id) {
                result.push_back(paper);
                break;
            }
        }
    }
    
    return result;
}

std::vector<Paper> ReviewManager::get_papers_by_reviewer(uint32_t reviewer_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        if (std::find(paper.reviewer_ids.begin(), paper.reviewer_ids.end(), reviewer_id) 
            != paper.reviewer_ids.end()) {
            result.push_back(paper);
        }
    }
    
    return result;
}

std::vector<Paper> ReviewManager::get_papers_by_status(PaperStatus status) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        if (pair.second.status == status) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<Paper> ReviewManager::get_all_papers() {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        result.push_back(pair.second);
    }
    
    return result;
}

bool ReviewManager::get_paper_status(uint32_t paper_id, uint32_t user_id,
                                     Paper& paper, std::vector<Review>& reviews) {
    if (!get_paper(paper_id, paper)) {
        return false;
    }
    
    reviews = get_reviews(paper_id, user_id);
    return true;
}

std::vector<PaperVersion> ReviewManager::get_paper_versions(uint32_t paper_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = paper_versions_.find(paper_id);
    if (it == paper_versions_.end()) {
        return {};
    }
    
    return it->second;
}

size_t ReviewManager::get_total_papers() const {
    std::lock_guard<std::mutex> lock(review_mutex_);
    return papers_.size();
}

ReviewManager::ReviewProgress ReviewManager::get_review_progress(uint32_t paper_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    ReviewProgress progress;
    progress.paper_id = paper_id;
    progress.total_reviewers = 0;
    progress.completed_reviews = 0;
    progress.progress_percentage = 0.0;
    progress.status = PaperStatus::SUBMITTED;
    
    auto paper_it = papers_.find(paper_id);
    if (paper_it != papers_.end()) {
        const Paper& paper = paper_it->second;
        progress.total_reviewers = paper.reviewer_ids.size();
        progress.status = paper.status;
        
        auto review_it = paper_reviews_.find(paper_id);
        if (review_it != paper_reviews_.end()) {
            progress.completed_reviews = review_it->second.size();
        }
        
        if (progress.total_reviewers > 0) {
            progress.progress_percentage = 
                static_cast<double>(progress.completed_reviews) / progress.total_reviewers * 100.0;
        }
    }
    
    return progress;
}

std::string ReviewManager::get_paper_path(uint32_t paper_id, uint32_t version) {
    std::stringstream ss;
    ss << get_data_dir() << "/" << paper_id << "/v" << version << ".pdf";
    return ss.str();
}

bool ReviewManager::save_data() {
    if (!fs_) return false;
    
    json j;
    j["next_paper_id"] = next_paper_id_;
    j["next_review_id"] = next_review_id_;
    j["next_decision_id"] = next_decision_id_;
    
    // 保存论文数据
    json papers_array = json::array();
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        json paper_json;
        paper_json["paper_id"] = paper.paper_id;
        paper_json["title"] = paper.title;
        paper_json["abstract"] = paper.abstract;
        paper_json["author_ids"] = paper.author_ids;
        paper_json["author_names"] = paper.author_names;
        paper_json["authors"] = paper.authors; // 新增
        paper_json["keywords"] = paper.keywords;
        paper_json["research_area"] = paper.research_area;
        paper_json["status"] = static_cast<int>(paper.status);
        paper_json["submit_time"] = paper.submit_time;
        paper_json["last_update"] = paper.last_update;
        paper_json["version"] = paper.version;
        paper_json["current_version_path"] = paper.current_version_path;
        paper_json["reviewer_ids"] = paper.reviewer_ids;
        paper_json["final_decision"] = static_cast<int>(paper.final_decision);
        paper_json["editor_id"] = paper.editor_id;
        paper_json["submitter_id"] = paper.submitter_id;
        
        papers_array.push_back(paper_json);
    }
    j["papers"] = papers_array;
    
    std::string data = j.dump();
    std::vector<uint8_t> bytes(data.begin(), data.end());
    
    // 确保数据文件存在
    if (!fs_->exists(get_db_path())) {
         fs_->create_file(get_db_path());
    }

    return fs_->write_file(get_db_path(), bytes, 0, false) >= 0;
}

bool ReviewManager::load_data() {
    if (!fs_) return false;
    
    if (!fs_->exists(get_db_path())) {
        return true; // 没有数据文件是正常的
    }
    
    std::vector<uint8_t> bytes;
    if (fs_->read_file(get_db_path(), bytes) < 0) {
        return false;
    }
    
    std::string data(bytes.begin(), bytes.end());
    
    try {
        json j = json::parse(data);
        
        next_paper_id_ = j["next_paper_id"];
        next_review_id_ = j["next_review_id"];
        next_decision_id_ = j["next_decision_id"];
        
        papers_.clear();
        
        for (const auto& paper_json : j["papers"]) {
            Paper paper;
            paper.paper_id = paper_json["paper_id"];
            paper.title = paper_json["title"];
            paper.abstract = paper_json["abstract"];
            paper.author_ids = paper_json["author_ids"].get<std::vector<uint32_t>>();
            if (paper_json.contains("author_names"))
                paper.author_names = paper_json["author_names"].get<std::vector<std::string>>();
            if (paper_json.contains("authors"))
                paper.authors = paper_json["authors"].get<std::vector<std::string>>();
            else paper.authors = paper.author_names;
                
            paper.keywords = paper_json["keywords"].get<std::vector<std::string>>();
            paper.research_area = paper_json["research_area"];
            paper.status = static_cast<PaperStatus>(paper_json["status"].get<int>());
            paper.submit_time = paper_json["submit_time"];
            paper.last_update = paper_json["last_update"];
            paper.version = paper_json["version"];
            paper.current_version_path = paper_json["current_version_path"];
            paper.reviewer_ids = paper_json["reviewer_ids"].get<std::vector<uint32_t>>();
            paper.final_decision = static_cast<DecisionType>(paper_json["final_decision"].get<int>());
            paper.editor_id = paper_json["editor_id"];
            if (paper_json.contains("submitter_id"))
                paper.submitter_id = paper_json["submitter_id"];
            else if (!paper.author_ids.empty())
                paper.submitter_id = paper.author_ids[0];
            
            papers_[paper.paper_id] = paper;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

// ==================== 工具函数实现 ====================
namespace ReviewUtils {
    std::string status_to_string(PaperStatus status) {
        switch (status) {
            case PaperStatus::SUBMITTED: return "SUBMITTED";
            case PaperStatus::UNDER_REVIEW: return "UNDER_REVIEW";
            case PaperStatus::REVISION_REQUIRED: return "REVISION_REQUIRED";
            case PaperStatus::REVISED: return "REVISED";
            case PaperStatus::ACCEPTED: return "ACCEPTED";
            case PaperStatus::REJECTED: return "REJECTED";
            default: return "UNKNOWN";
        }
    }
    
    std::string decision_to_string(DecisionType decision) {
        switch (decision) {
            case DecisionType::ACCEPT: return "ACCEPT";
            case DecisionType::REJECT: return "REJECT";
            case DecisionType::MAJOR_REVISION: return "MAJOR_REVISION";
            case DecisionType::MINOR_REVISION: return "MINOR_REVISION";
            default: return "UNKNOWN";
        }
    }
    
    std::string generate_paper_id(uint32_t id) {
        std::stringstream ss;
        ss << "P" << std::setfill('0') << std::setw(6) << id;
        return ss.str();
    }
}

// ==================== API 支持方法 ====================

std::vector<Review> ReviewManager::get_paper_reviews(uint32_t paper_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    auto it = paper_reviews_.find(paper_id);
    if (it != paper_reviews_.end()) {
        return it->second;
    }
    return {};
}

std::vector<Paper> ReviewManager::get_reviewer_papers(uint32_t reviewer_id) {
    std::lock_guard<std::mutex> lock(review_mutex_);
    
    std::vector<Paper> result;
    for (const auto& pair : papers_) {
        const Paper& paper = pair.second;
        for (uint32_t rid : paper.reviewer_ids) {
            if (rid == reviewer_id) {
                result.push_back(paper);
                break;
            }
        }
    }
    return result;
}