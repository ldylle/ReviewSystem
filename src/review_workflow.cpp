#include "review_workflow.h"
#include "terminal_ui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

// ==================== ReviewWorkflowManager å®žçŽ° ====================
ReviewWorkflowManager::ReviewWorkflowManager()
    : next_task_id_(1), next_notification_id_(1) {
}

void ReviewWorkflowManager::set_deadline(uint32_t paper_id, ReviewRound round,
                                         time_t review_deadline, time_t revision_deadline) {
    Deadline dl;
    dl.paper_id = paper_id;
    dl.round = round;
    dl.submission_deadline = review_deadline;
    dl.revision_deadline = revision_deadline > 0 ? revision_deadline : 
                           review_deadline + config_.default_revision_days * 86400;
    dl.decision_deadline = dl.submission_deadline + 7 * 86400;  // è¯„å®¡åŽ7å¤©å†…å†³å®š
    
    deadlines_[paper_id] = dl;
}

bool ReviewWorkflowManager::extend_deadline(uint32_t paper_id, int extra_days, 
                                           const std::string& reason) {
    auto it = deadlines_.find(paper_id);
    if (it == deadlines_.end()) return false;
    
    it->second.submission_deadline += extra_days * 86400;
    it->second.is_extended = true;
    it->second.extension_reason = reason;
    
    return true;
}

Deadline ReviewWorkflowManager::get_deadline(uint32_t paper_id) const {
    auto it = deadlines_.find(paper_id);
    if (it != deadlines_.end()) return it->second;
    return Deadline();
}

std::vector<uint32_t> ReviewWorkflowManager::get_overdue_papers() const {
    std::vector<uint32_t> result;
    time_t now = time(nullptr);
    
    for (const auto& [pid, dl] : deadlines_) {
        if (now > dl.submission_deadline) {
            result.push_back(pid);
        }
    }
    return result;
}

std::vector<uint32_t> ReviewWorkflowManager::get_approaching_deadline_papers(int days) const {
    std::vector<uint32_t> result;
    time_t now = time(nullptr);
    time_t threshold = now + days * 86400;
    
    for (const auto& [pid, dl] : deadlines_) {
        if (dl.submission_deadline > now && dl.submission_deadline <= threshold) {
            result.push_back(pid);
        }
    }
    return result;
}

uint32_t ReviewWorkflowManager::create_task(uint32_t paper_id, uint32_t reviewer_id, 
                                            ReviewRound round) {
    ReviewTask task;
    task.task_id = next_task_id_++;
    task.paper_id = paper_id;
    task.reviewer_id = reviewer_id;
    task.round = round;
    task.assigned_time = time(nullptr);
    task.deadline = task.assigned_time + config_.default_review_days * 86400;
    task.is_completed = false;
    
    tasks_[task.task_id] = task;
    
    // å‘é€é€šçŸ¥
    send_notification(reviewer_id, NotificationType::PAPER_ASSIGNED, paper_id,
                     "You have been assigned to review paper #" + std::to_string(paper_id));
    
    return task.task_id;
}

bool ReviewWorkflowManager::complete_task(uint32_t task_id, int score, 
                                         const std::string& comments, int recommendation) {
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) return false;
    
    ReviewTask& task = it->second;
    task.is_completed = true;
    task.completed_time = time(nullptr);
    
    // æ›´æ–°è½®æ¬¡è®°å½•
    uint32_t pid = task.paper_id;
    ReviewRound round = task.round;
    
    auto round_it = paper_rounds_.find(pid);
    if (round_it != paper_rounds_.end()) {
        for (auto& r : round_it->second) {
            if (r.round == round) {
                r.scores[task.reviewer_id] = score;
                r.comments[task.reviewer_id] = comments;
                r.recommendations[task.reviewer_id] = recommendation;
                break;
            }
        }
    }
    
    // æ£€æŸ¥æ˜¯å¦æ‰€æœ‰è¯„å®¡éƒ½å®Œæˆ
    auto tasks = get_tasks_by_paper(pid);
    bool all_complete = true;
    for (const auto& t : tasks) {
        if (!t.is_completed && !t.is_declined) {
            all_complete = false;
            break;
        }
    }
    
    if (all_complete) {
        // é€šçŸ¥ç¼–è¾‘
        send_notification(0, NotificationType::ALL_REVIEWS_COMPLETE, pid,
                         "All reviews completed for paper #" + std::to_string(pid));
    }
    
    return true;
}

bool ReviewWorkflowManager::decline_task(uint32_t task_id, const std::string& reason) {
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) return false;
    
    it->second.is_declined = true;
    it->second.decline_reason = reason;
    
    return true;
}

ReviewTask ReviewWorkflowManager::get_task(uint32_t task_id) const {
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) return it->second;
    return ReviewTask();
}

std::vector<ReviewTask> ReviewWorkflowManager::get_tasks_by_reviewer(uint32_t reviewer_id) const {
    std::vector<ReviewTask> result;
    for (const auto& [tid, task] : tasks_) {
        if (task.reviewer_id == reviewer_id) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<ReviewTask> ReviewWorkflowManager::get_tasks_by_paper(uint32_t paper_id) const {
    std::vector<ReviewTask> result;
    for (const auto& [tid, task] : tasks_) {
        if (task.paper_id == paper_id) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<ReviewTask> ReviewWorkflowManager::get_overdue_tasks() const {
    std::vector<ReviewTask> result;
    for (const auto& [tid, task] : tasks_) {
        if (task.is_overdue()) {
            result.push_back(task);
        }
    }
    return result;
}

bool ReviewWorkflowManager::start_round(uint32_t paper_id, ReviewRound round,
                                        const std::vector<uint32_t>& reviewer_ids) {
    RoundReview rr;
    rr.paper_id = paper_id;
    rr.round = round;
    rr.reviewer_ids = reviewer_ids;
    rr.start_time = time(nullptr);
    
    paper_rounds_[paper_id].push_back(rr);
    
    // åˆ›å»ºå®¡ç¨¿ä»»åŠ¡
    for (uint32_t rid : reviewer_ids) {
        create_task(paper_id, rid, round);
    }
    
    // è®¾ç½®æˆªæ­¢æ—¥æœŸ
    set_deadline(paper_id, round, time(nullptr) + config_.default_review_days * 86400);
    
    return true;
}

bool ReviewWorkflowManager::end_round(uint32_t paper_id, ReviewRound round,
                                      const std::string& decision, 
                                      const std::string& editor_comments) {
    auto it = paper_rounds_.find(paper_id);
    if (it == paper_rounds_.end()) return false;
    
    for (auto& r : it->second) {
        if (r.round == round) {
            r.end_time = time(nullptr);
            r.round_decision = decision;
            r.editor_comments = editor_comments;
            return true;
        }
    }
    return false;
}

RoundReview ReviewWorkflowManager::get_round(uint32_t paper_id, ReviewRound round) const {
    auto it = paper_rounds_.find(paper_id);
    if (it != paper_rounds_.end()) {
        for (const auto& r : it->second) {
            if (r.round == round) return r;
        }
    }
    return RoundReview();
}

std::vector<RoundReview> ReviewWorkflowManager::get_all_rounds(uint32_t paper_id) const {
    auto it = paper_rounds_.find(paper_id);
    if (it != paper_rounds_.end()) return it->second;
    return {};
}

ReviewRound ReviewWorkflowManager::get_current_round(uint32_t paper_id) const {
    auto it = paper_rounds_.find(paper_id);
    if (it != paper_rounds_.end() && !it->second.empty()) {
        return it->second.back().round;
    }
    return ReviewRound::FIRST_ROUND;
}

ReviewWorkflowManager::PaperProgress ReviewWorkflowManager::get_progress(uint32_t paper_id) const {
    PaperProgress progress;
    progress.paper_id = paper_id;
    progress.current_round = get_current_round(paper_id);
    
    auto tasks = get_tasks_by_paper(paper_id);
    progress.total_reviewers = 0;
    progress.completed_reviews = 0;
    
    for (const auto& task : tasks) {
        if (!task.is_declined) {
            progress.total_reviewers++;
            if (task.is_completed) {
                progress.completed_reviews++;
            } else {
                progress.pending_reviewers.push_back("Reviewer #" + std::to_string(task.reviewer_id));
            }
        }
    }
    
    auto round = get_round(paper_id, progress.current_round);
    progress.average_score = round.average_score();
    
    auto dl = get_deadline(paper_id);
    progress.days_until_deadline = dl.days_remaining();
    
    if (round.start_time > 0) {
        progress.days_in_review = (time(nullptr) - round.start_time) / 86400;
    }
    
    // ç¡®å®šçŠ¶æ€
    if (progress.completed_reviews == progress.total_reviewers && progress.total_reviewers > 0) {
        progress.status = "READY_FOR_DECISION";
    } else if (progress.days_until_deadline < 0) {
        progress.status = "OVERDUE";
    } else if (progress.days_until_deadline <= 3) {
        progress.status = "URGENT";
    } else {
        progress.status = "IN_PROGRESS";
    }
    
    return progress;
}

std::string ReviewWorkflowManager::render_progress_chart(uint32_t paper_id) const {
    auto progress = get_progress(paper_id);
    std::stringstream ss;
    
    ss << Color::CYAN << "\n  Review Progress for Paper #" << paper_id << "\n";
    ss << Color::BRIGHT_BLACK << "  " << std::string(50, '-') << "\n" << Color::RESET;
    
    // è¿›åº¦æ¡
    double completion = progress.total_reviewers > 0 ? 
        static_cast<double>(progress.completed_reviews) / progress.total_reviewers : 0;
    ss << "  " << TerminalUI::progress_bar(completion, 30, 
        std::to_string(progress.completed_reviews) + "/" + 
        std::to_string(progress.total_reviewers) + " reviews");
    ss << "\n\n";
    
    // çŠ¶æ€
    ss << "  Status: ";
    if (progress.status == "READY_FOR_DECISION") {
        ss << Color::GREEN << Symbol::CHECK << " Ready for Decision" << Color::RESET;
    } else if (progress.status == "OVERDUE") {
        ss << Color::RED << Symbol::WARNING << " OVERDUE" << Color::RESET;
    } else if (progress.status == "URGENT") {
        ss << Color::YELLOW << Symbol::WARNING << " Urgent (" << progress.days_until_deadline << " days left)" << Color::RESET;
    } else {
        ss << Color::BLUE << Symbol::CIRCLE << " In Progress" << Color::RESET;
    }
    ss << "\n";
    
    // æ—¶é—´ä¿¡æ¯
    ss << "  Days in Review: " << progress.days_in_review << "\n";
    ss << "  Days Until Deadline: ";
    if (progress.days_until_deadline < 0) {
        ss << Color::RED << progress.days_until_deadline << " (OVERDUE)" << Color::RESET;
    } else if (progress.days_until_deadline <= 3) {
        ss << Color::YELLOW << progress.days_until_deadline << Color::RESET;
    } else {
        ss << Color::GREEN << progress.days_until_deadline << Color::RESET;
    }
    ss << "\n";
    
    // å¹³å‡åˆ†
    if (progress.completed_reviews > 0) {
        ss << "  Average Score: " << std::fixed << std::setprecision(1) << progress.average_score << "/10\n";
    }
    
    // å¾…å®¡ç¨¿äºº
    if (!progress.pending_reviewers.empty()) {
        ss << "\n  " << Color::YELLOW << "Pending Reviewers:" << Color::RESET << "\n";
        for (const auto& r : progress.pending_reviewers) {
            ss << "    " << Symbol::BULLET << " " << r << "\n";
        }
    }
    
    ss << "\n";
    return ss.str();
}

void ReviewWorkflowManager::set_notification_callback(
    std::function<void(const Notification&)> callback) {
    notification_callback_ = callback;
}

void ReviewWorkflowManager::send_notification(uint32_t recipient_id, NotificationType type,
                                              uint32_t paper_id, const std::string& message) {
    Notification notif;
    notif.notification_id = next_notification_id_++;
    notif.recipient_id = recipient_id;
    notif.type = type;
    notif.paper_id = paper_id;
    notif.title = notification_type_to_string(type);
    notif.message = message;
    notif.created_time = time(nullptr);
    
    notifications_.push_back(notif);
    
    if (notification_callback_) {
        notification_callback_(notif);
    }
}

std::vector<Notification> ReviewWorkflowManager::get_notifications(uint32_t user_id, 
                                                                   bool unread_only) const {
    std::vector<Notification> result;
    for (const auto& n : notifications_) {
        if (n.recipient_id == user_id || n.recipient_id == 0) {
            if (!unread_only || !n.is_read) {
                result.push_back(n);
            }
        }
    }
    return result;
}

void ReviewWorkflowManager::mark_notification_read(uint32_t notification_id) {
    for (auto& n : notifications_) {
        if (n.notification_id == notification_id) {
            n.is_read = true;
            break;
        }
    }
}

void ReviewWorkflowManager::process_reminders() {
    if (!config_.auto_remind) return;
    
    time_t now = time(nullptr);
    
    for (auto& [tid, task] : tasks_) {
        if (task.is_completed || task.is_declined) continue;
        
        // æ£€æŸ¥æ˜¯å¦éœ€è¦å‘é€æé†’
        int days_left = (task.deadline - now) / 86400;
        
        if (days_left <= config_.reminder_interval_days) {
            // æ£€æŸ¥ä¸Šæ¬¡æé†’æ—¶é—´
            if (task.reminder_count < config_.max_reminders &&
                (task.last_reminder == 0 || 
                 now - task.last_reminder >= config_.reminder_interval_days * 86400)) {
                
                send_notification(task.reviewer_id, NotificationType::REVIEW_REMINDER,
                                 task.paper_id,
                                 "Reminder: Your review for paper #" + 
                                 std::to_string(task.paper_id) + " is due in " +
                                 std::to_string(days_left) + " days");
                
                task.reminder_count++;
                task.last_reminder = now;
            }
        }
        
        // æ£€æŸ¥æ˜¯å¦è¶…æ—¶
        if (days_left < 0 && task.reminder_count == 0) {
            send_notification(task.reviewer_id, NotificationType::REVIEW_OVERDUE,
                             task.paper_id,
                             "OVERDUE: Your review for paper #" + 
                             std::to_string(task.paper_id) + " is overdue!");
        }
    }
}

void ReviewWorkflowManager::process_escalations() {
    if (!config_.auto_escalate) return;
    
    // å¤„ç†è¶…æ—¶å‡çº§é€»è¾‘
    // ...
}

void ReviewWorkflowManager::daily_maintenance() {
    process_reminders();
    process_escalations();
}

ReviewWorkflowManager::WorkflowStatistics ReviewWorkflowManager::get_statistics() const {
    WorkflowStatistics stats;
    stats.active_papers = paper_rounds_.size();
    stats.total_tasks = tasks_.size();
    stats.completed_tasks = 0;
    stats.overdue_tasks = 0;
    
    int total_time = 0;
    int completed_count = 0;
    
    for (const auto& [tid, task] : tasks_) {
        if (task.is_completed) {
            stats.completed_tasks++;
            if (task.completed_time > 0 && task.assigned_time > 0) {
                total_time += (task.completed_time - task.assigned_time);
                completed_count++;
            }
        } else if (task.is_overdue()) {
            stats.overdue_tasks++;
        }
    }
    
    stats.average_review_time = completed_count > 0 ? 
        (total_time / completed_count / 86400.0) : 0;
    stats.on_time_rate = stats.total_tasks > 0 ?
        static_cast<double>(stats.completed_tasks - stats.overdue_tasks) / stats.total_tasks : 0;
    
    for (const auto& [pid, rounds] : paper_rounds_) {
        if (!rounds.empty()) {
            stats.papers_per_round[rounds.back().round]++;
        }
    }
    
    return stats;
}

std::string ReviewWorkflowManager::notification_type_to_string(NotificationType type) const {
    switch (type) {
        case NotificationType::PAPER_ASSIGNED: return "Paper Assigned";
        case NotificationType::REVIEW_REMINDER: return "Review Reminder";
        case NotificationType::REVIEW_OVERDUE: return "Review Overdue";
        case NotificationType::REVIEW_SUBMITTED: return "Review Submitted";
        case NotificationType::ALL_REVIEWS_COMPLETE: return "All Reviews Complete";
        case NotificationType::DECISION_MADE: return "Decision Made";
        case NotificationType::REVISION_REQUIRED: return "Revision Required";
        case NotificationType::REVISION_SUBMITTED: return "Revision Submitted";
        case NotificationType::PAPER_ACCEPTED: return "Paper Accepted";
        case NotificationType::PAPER_REJECTED: return "Paper Rejected";
        default: return "Notification";
    }
}

// ==================== ReviewTemplate å®žçŽ° ====================
ReviewTemplate ReviewTemplate::standard_template() {
    ReviewTemplate t;
    t.template_name = "Standard Review";
    t.criteria = {"Originality", "Technical Quality", "Clarity", "Significance", "Relevance"};
    t.weights = {{"Originality", 25}, {"Technical Quality", 30}, {"Clarity", 15}, 
                {"Significance", 20}, {"Relevance", 10}};
    t.required_sections = {"Summary", "Strengths", "Weaknesses", "Detailed Comments"};
    t.guidelines = "Please provide a comprehensive review covering all criteria.";
    return t;
}

ReviewTemplate ReviewTemplate::double_blind_template() {
    ReviewTemplate t = standard_template();
    t.template_name = "Double-Blind Review";
    t.guidelines = "This is a double-blind review. Do not attempt to identify the authors.";
    return t;
}

ReviewTemplate ReviewTemplate::technical_paper_template() {
    ReviewTemplate t;
    t.template_name = "Technical Paper Review";
    t.criteria = {"Novelty", "Technical Correctness", "Reproducibility", 
                  "Presentation", "Impact", "Related Work"};
    t.weights = {{"Novelty", 20}, {"Technical Correctness", 25}, {"Reproducibility", 15},
                {"Presentation", 15}, {"Impact", 15}, {"Related Work", 10}};
    t.required_sections = {"Summary", "Technical Assessment", "Reproducibility Assessment",
                          "Strengths", "Weaknesses", "Questions for Authors", "Recommendation"};
    return t;
}

// ==================== WorkflowUtils å®žçŽ° ====================
namespace WorkflowUtils {
    std::string round_to_string(ReviewRound round) {
        switch (round) {
            case ReviewRound::FIRST_ROUND: return "First Round";
            case ReviewRound::SECOND_ROUND: return "Second Round";
            case ReviewRound::THIRD_ROUND: return "Third Round";
            case ReviewRound::FINAL_ROUND: return "Final Round";
            default: return "Unknown";
        }
    }
    
    ReviewRound string_to_round(const std::string& str) {
        if (str == "1" || str == "FIRST" || str == "First Round") 
            return ReviewRound::FIRST_ROUND;
        if (str == "2" || str == "SECOND" || str == "Second Round")
            return ReviewRound::SECOND_ROUND;
        if (str == "3" || str == "THIRD" || str == "Third Round")
            return ReviewRound::THIRD_ROUND;
        return ReviewRound::FIRST_ROUND;
    }
    
    std::string recommendation_to_string(int rec) {
        switch (rec) {
            case 1: return "Accept";
            case 2: return "Minor Revision";
            case 3: return "Major Revision";
            case 4: return "Reject";
            default: return "Unknown";
        }
    }
    
    std::string format_deadline(time_t deadline) {
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&deadline));
        
        int days = (deadline - time(nullptr)) / 86400;
        std::string result = buffer;
        
        if (days < 0) {
            result += " (OVERDUE)";
        } else if (days == 0) {
            result += " (TODAY)";
        } else if (days == 1) {
            result += " (TOMORROW)";
        } else if (days <= 7) {
            result += " (" + std::to_string(days) + " days)";
        }
        
        return result;
    }
    
    std::string format_duration(int days) {
        if (days < 7) return std::to_string(days) + " days";
        if (days < 30) return std::to_string(days / 7) + " weeks";
        return std::to_string(days / 30) + " months";
    }
    
    std::string generate_review_summary(const std::vector<ReviewForm>& reviews) {
        if (reviews.empty()) return "No reviews available.";
        
        std::stringstream ss;
        
        double avg_score = 0;
        std::map<int, int> rec_counts;
        
        for (const auto& r : reviews) {
            avg_score += r.overall_score;
            rec_counts[r.recommendation]++;
        }
        avg_score /= reviews.size();
        
        ss << "Summary of " << reviews.size() << " Reviews:\n";
        ss << "  Average Score: " << std::fixed << std::setprecision(1) << avg_score << "/10\n";
        ss << "  Recommendations:\n";
        
        for (const auto& [rec, count] : rec_counts) {
            ss << "    - " << recommendation_to_string(rec) << ": " << count << "\n";
        }
        
        return ss.str();
    }
    
    int suggest_decision(const std::vector<ReviewForm>& reviews) {
        if (reviews.empty()) return 0;
        
        double avg_score = 0;
        int accept_count = 0;
        int reject_count = 0;
        
        for (const auto& r : reviews) {
            avg_score += r.overall_score;
            if (r.recommendation == 1) accept_count++;
            if (r.recommendation == 4) reject_count++;
        }
        avg_score /= reviews.size();
        
        // ç®€å•å†³ç­–é€»è¾‘
        if (avg_score >= 7.0 && reject_count == 0) return 1;  // Accept
        if (avg_score < 4.0 || static_cast<size_t>(reject_count) > reviews.size() / 2) return 4;  // Reject
        if (avg_score >= 5.0) return 2;  // Minor Revision
        return 3;  // Major Revision
    }
}