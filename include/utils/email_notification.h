#ifndef EMAIL_NOTIFICATION_H
#define EMAIL_NOTIFICATION_H

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>
#include <sstream>
#include <ctime>
#include <regex>
#include <iostream>

// ==================== é‚®ä»¶é…ç½® ====================
struct SMTPConfig {
    std::string server;
    int port;
    std::string username;
    std::string password;
    bool use_tls;
    std::string from_address;
    std::string from_name;
    int timeout_seconds;
    int max_retries;
    
    SMTPConfig() : port(587), use_tls(true), timeout_seconds(30), max_retries(3) {}
};

// ==================== é‚®ä»¶ä¼˜å…ˆçº§ ====================
enum class EmailPriority {
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    URGENT = 4
};

// ==================== é‚®ä»¶çŠ¶æ€ ====================
enum class EmailStatus {
    PENDING,
    SENDING,
    SENT,
    FAILED,
    RETRYING
};

// ==================== é‚®ä»¶é™„ä»¶ ====================
struct EmailAttachment {
    std::string filename;
    std::string mime_type;
    std::vector<uint8_t> content;
    
    EmailAttachment() = default;
    EmailAttachment(const std::string& name, const std::string& mime, const std::vector<uint8_t>& data)
        : filename(name), mime_type(mime), content(data) {}
};

// ==================== é‚®ä»¶æ¶ˆæ¯ ====================
struct EmailMessage {
    uint64_t message_id;
    std::string to;
    std::vector<std::string> cc;
    std::vector<std::string> bcc;
    std::string subject;
    std::string body_text;
    std::string body_html;
    std::vector<EmailAttachment> attachments;
    EmailPriority priority;
    EmailStatus status;
    time_t created_time;
    time_t sent_time;
    int retry_count;
    std::string error_message;
    std::map<std::string, std::string> headers;
    
    EmailMessage() : message_id(0), priority(EmailPriority::NORMAL), 
                     status(EmailStatus::PENDING), created_time(time(nullptr)),
                     sent_time(0), retry_count(0) {}
};

// ==================== é‚®ä»¶æ¨¡æ¿ç±»åž‹ ====================
enum class EmailTemplateType {
    PAPER_SUBMITTED,
    PAPER_ASSIGNED,
    REVIEW_REMINDER,
    REVIEW_RECEIVED,
    DECISION_MADE,
    REVISION_REQUIRED,
    PAPER_ACCEPTED,
    PAPER_REJECTED,
    DEADLINE_WARNING,
    ACCOUNT_CREATED,
    PASSWORD_RESET,
    SYSTEM_NOTIFICATION
};

// ==================== é‚®ä»¶æ¨¡æ¿ ====================
class EmailTemplate {
public:
    EmailTemplateType type;
    std::string name;
    std::string subject_template;
    std::string body_template;
    std::string html_template;
    
    EmailTemplate() : type(EmailTemplateType::SYSTEM_NOTIFICATION) {}
    
    // æ¸²æŸ“æ¨¡æ¿ - æ›¿æ¢å˜é‡
    std::string render(const std::string& tmpl, const std::map<std::string, std::string>& vars) const {
        std::string result = tmpl;
        for (const auto& [key, value] : vars) {
            std::string placeholder = "{{" + key + "}}";
            size_t pos;
            while ((pos = result.find(placeholder)) != std::string::npos) {
                result.replace(pos, placeholder.length(), value);
            }
        }
        return result;
    }
    
    EmailMessage createMessage(const std::map<std::string, std::string>& vars) const {
        EmailMessage msg;
        msg.subject = render(subject_template, vars);
        msg.body_text = render(body_template, vars);
        msg.body_html = render(html_template, vars);
        return msg;
    }
};

// ==================== é‚®ä»¶æ¨¡æ¿ç®¡ç†å™¨ ====================
class EmailTemplateManager {
private:
    std::map<EmailTemplateType, EmailTemplate> templates_;
    
public:
    EmailTemplateManager() {
        initializeDefaultTemplates();
    }
    
    void initializeDefaultTemplates() {
        // è®ºæ–‡æäº¤ç¡®è®¤
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::PAPER_SUBMITTED;
            tmpl.name = "Paper Submission Confirmation";
            tmpl.subject_template = "[ReviewSystem] Paper Submitted: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{author_name}},

Your paper has been successfully submitted to our review system.

Paper Details:
- Paper ID: {{paper_id}}
- Title: {{paper_title}}
- Submission Time: {{submit_time}}
- Status: Pending Review

You can track the status of your submission through the system.

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .info-box { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .info-row { display: flex; padding: 10px 0; border-bottom: 1px solid #eee; }
        .info-label { font-weight: bold; width: 150px; color: #666; }
        .info-value { flex: 1; }
        .status-badge { display: inline-block; padding: 5px 15px; background: #ffc107; color: #333; border-radius: 20px; font-weight: bold; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸ“ Paper Submitted</h1>
            <p>Your submission has been received</p>
        </div>
        <div class="content">
            <p>Dear <strong>{{author_name}}</strong>,</p>
            <p>Your paper has been successfully submitted to our review system.</p>
            
            <div class="info-box">
                <h3>ðŸ“‹ Paper Details</h3>
                <div class="info-row">
                    <span class="info-label">Paper ID:</span>
                    <span class="info-value">{{paper_id}}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Title:</span>
                    <span class="info-value">{{paper_title}}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Submission Time:</span>
                    <span class="info-value">{{submit_time}}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Status:</span>
                    <span class="info-value"><span class="status-badge">Pending Review</span></span>
                </div>
            </div>
            
            <p>You can track the status of your submission through the system.</p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
            <p>Â© 2025 Scientific Review System</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
        
        // å®¡ç¨¿åˆ†é…é€šçŸ¥
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::PAPER_ASSIGNED;
            tmpl.name = "Review Assignment";
            tmpl.subject_template = "[ReviewSystem] New Paper Assigned for Review: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{reviewer_name}},

You have been assigned to review a new paper.

Paper Details:
- Paper ID: {{paper_id}}
- Title: {{paper_title}}
- Research Area: {{research_area}}
- Deadline: {{deadline}}

Please log in to the system to access the paper and submit your review.

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .info-box { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .deadline-box { background: #fff3cd; border-left: 4px solid #ffc107; padding: 15px; margin: 20px 0; }
        .btn { display: inline-block; padding: 12px 30px; background: #11998e; color: white; text-decoration: none; border-radius: 5px; font-weight: bold; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸ“š Review Assignment</h1>
            <p>You have a new paper to review</p>
        </div>
        <div class="content">
            <p>Dear <strong>{{reviewer_name}}</strong>,</p>
            <p>You have been assigned to review a new paper in your area of expertise.</p>
            
            <div class="info-box">
                <h3>ðŸ“‹ Paper Details</h3>
                <p><strong>Paper ID:</strong> {{paper_id}}</p>
                <p><strong>Title:</strong> {{paper_title}}</p>
                <p><strong>Research Area:</strong> {{research_area}}</p>
            </div>
            
            <div class="deadline-box">
                <strong>â° Review Deadline:</strong> {{deadline}}
            </div>
            
            <p style="text-align: center; margin-top: 30px;">
                <a href="#" class="btn">Access Paper</a>
            </p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
        
        // å®¡ç¨¿æé†’
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::REVIEW_REMINDER;
            tmpl.name = "Review Reminder";
            tmpl.subject_template = "[REMINDER] Review Deadline Approaching: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{reviewer_name}},

This is a friendly reminder that your review for the following paper is due soon.

Paper: {{paper_title}} (ID: {{paper_id}})
Deadline: {{deadline}}
Days Remaining: {{days_remaining}}

Please submit your review before the deadline.

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .warning-box { background: #f8d7da; border: 1px solid #f5c6cb; padding: 20px; border-radius: 8px; margin: 20px 0; }
        .countdown { font-size: 48px; font-weight: bold; color: #dc3545; text-align: center; margin: 20px 0; }
        .btn { display: inline-block; padding: 12px 30px; background: #dc3545; color: white; text-decoration: none; border-radius: 5px; font-weight: bold; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>â° Review Reminder</h1>
            <p>Your deadline is approaching</p>
        </div>
        <div class="content">
            <p>Dear <strong>{{reviewer_name}}</strong>,</p>
            
            <div class="warning-box">
                <p>âš ï¸ <strong>Reminder:</strong> Your review deadline is approaching!</p>
            </div>
            
            <p><strong>Paper:</strong> {{paper_title}}</p>
            <p><strong>Paper ID:</strong> {{paper_id}}</p>
            <p><strong>Deadline:</strong> {{deadline}}</p>
            
            <div class="countdown">{{days_remaining}} days left</div>
            
            <p style="text-align: center; margin-top: 30px;">
                <a href="#" class="btn">Submit Review Now</a>
            </p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
        
        // è®ºæ–‡æŽ¥å—é€šçŸ¥
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::PAPER_ACCEPTED;
            tmpl.name = "Paper Accepted";
            tmpl.subject_template = "ðŸŽ‰ [ReviewSystem] Congratulations! Paper Accepted: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{author_name}},

Congratulations! We are pleased to inform you that your paper has been ACCEPTED.

Paper Details:
- Paper ID: {{paper_id}}
- Title: {{paper_title}}
- Decision Date: {{decision_date}}

Editor's Comments:
{{editor_comments}}

Thank you for your contribution to our community!

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .success-box { background: #d4edda; border: 1px solid #c3e6cb; padding: 20px; border-radius: 8px; margin: 20px 0; text-align: center; }
        .success-icon { font-size: 64px; margin-bottom: 10px; }
        .comments-box { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #28a745; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸŽ‰ Congratulations!</h1>
            <p>Your paper has been accepted</p>
        </div>
        <div class="content">
            <p>Dear <strong>{{author_name}}</strong>,</p>
            
            <div class="success-box">
                <div class="success-icon">âœ…</div>
                <h2 style="color: #28a745; margin: 0;">PAPER ACCEPTED</h2>
            </div>
            
            <p><strong>Paper ID:</strong> {{paper_id}}</p>
            <p><strong>Title:</strong> {{paper_title}}</p>
            <p><strong>Decision Date:</strong> {{decision_date}}</p>
            
            <div class="comments-box">
                <h4>ðŸ“ Editor's Comments:</h4>
                <p>{{editor_comments}}</p>
            </div>
            
            <p>Thank you for your valuable contribution to our community!</p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
            <p>Â© 2025 Scientific Review System</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
        
        // è®ºæ–‡æ‹’ç»é€šçŸ¥
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::PAPER_REJECTED;
            tmpl.name = "Paper Rejected";
            tmpl.subject_template = "[ReviewSystem] Decision on Paper: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{author_name}},

Thank you for submitting your paper to our system. After careful consideration, we regret to inform you that your paper has not been accepted.

Paper Details:
- Paper ID: {{paper_id}}
- Title: {{paper_title}}
- Decision Date: {{decision_date}}

Editor's Comments:
{{editor_comments}}

We encourage you to consider the feedback provided and submit a revised version in the future.

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .info-box { background: #f8d7da; border: 1px solid #f5c6cb; padding: 20px; border-radius: 8px; margin: 20px 0; }
        .comments-box { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #6c757d; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸ“‹ Decision Notice</h1>
            <p>Regarding your submission</p>
        </div>
        <div class="content">
            <p>Dear <strong>{{author_name}}</strong>,</p>
            <p>Thank you for submitting your paper to our system. After careful consideration by our reviewers, we regret to inform you that your paper has not been accepted at this time.</p>
            
            <div class="info-box">
                <p><strong>Paper ID:</strong> {{paper_id}}</p>
                <p><strong>Title:</strong> {{paper_title}}</p>
                <p><strong>Decision Date:</strong> {{decision_date}}</p>
            </div>
            
            <div class="comments-box">
                <h4>ðŸ“ Editor's Comments:</h4>
                <p>{{editor_comments}}</p>
            </div>
            
            <p>We encourage you to consider the feedback provided and welcome a revised submission in the future.</p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
        
        // éœ€è¦ä¿®è®¢é€šçŸ¥
        {
            EmailTemplate tmpl;
            tmpl.type = EmailTemplateType::REVISION_REQUIRED;
            tmpl.name = "Revision Required";
            tmpl.subject_template = "[ReviewSystem] Revision Required: {{paper_title}}";
            tmpl.body_template = R"(
Dear {{author_name}},

Your paper requires revision before a final decision can be made.

Paper Details:
- Paper ID: {{paper_id}}
- Title: {{paper_title}}
- Revision Deadline: {{revision_deadline}}
- Revision Type: {{revision_type}}

Editor's Comments:
{{editor_comments}}

Reviewer Feedback:
{{reviewer_feedback}}

Please submit your revised paper before the deadline.

Best regards,
Review System Team
)";
            tmpl.html_template = R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white; padding: 30px; text-align: center; border-radius: 10px 10px 0 0; }
        .content { background: #f9f9f9; padding: 30px; border-radius: 0 0 10px 10px; }
        .revision-badge { display: inline-block; padding: 8px 20px; background: #ffc107; color: #333; border-radius: 20px; font-weight: bold; }
        .deadline-box { background: #fff3cd; border-left: 4px solid #ffc107; padding: 15px; margin: 20px 0; }
        .feedback-box { background: white; padding: 20px; border-radius: 8px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .btn { display: inline-block; padding: 12px 30px; background: #f5576c; color: white; text-decoration: none; border-radius: 5px; font-weight: bold; }
        .footer { text-align: center; padding: 20px; color: #999; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸ“ Revision Required</h1>
            <p><span class="revision-badge">{{revision_type}}</span></p>
        </div>
        <div class="content">
            <p>Dear <strong>{{author_name}}</strong>,</p>
            <p>Your paper requires revision before a final decision can be made.</p>
            
            <p><strong>Paper ID:</strong> {{paper_id}}</p>
            <p><strong>Title:</strong> {{paper_title}}</p>
            
            <div class="deadline-box">
                <strong>â° Revision Deadline:</strong> {{revision_deadline}}
            </div>
            
            <div class="feedback-box">
                <h4>ðŸ“ Editor's Comments:</h4>
                <p>{{editor_comments}}</p>
            </div>
            
            <div class="feedback-box">
                <h4>ðŸ‘¥ Reviewer Feedback:</h4>
                <p>{{reviewer_feedback}}</p>
            </div>
            
            <p style="text-align: center; margin-top: 30px;">
                <a href="#" class="btn">Submit Revision</a>
            </p>
        </div>
        <div class="footer">
            <p>Best regards,<br>Review System Team</p>
        </div>
    </div>
</body>
</html>
)";
            templates_[tmpl.type] = tmpl;
        }
    }
    
    EmailTemplate* getTemplate(EmailTemplateType type) {
        auto it = templates_.find(type);
        if (it != templates_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    void addTemplate(const EmailTemplate& tmpl) {
        templates_[tmpl.type] = tmpl;
    }
};

// ==================== é‚®ä»¶æœåŠ¡æŽ¥å£ ====================
class IEmailService {
public:
    virtual ~IEmailService() = default;
    virtual bool send(const EmailMessage& message) = 0;
    virtual bool isConfigured() const = 0;
};

// ==================== æ¨¡æ‹Ÿé‚®ä»¶æœåŠ¡(ç”¨äºŽæµ‹è¯•) ====================
class MockEmailService : public IEmailService {
private:
    std::vector<EmailMessage> sent_emails_;
    mutable std::mutex mutex_;
    bool simulate_failure_;
    
public:
    MockEmailService() : simulate_failure_(false) {}
    
    bool send(const EmailMessage& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (simulate_failure_) return false;
        sent_emails_.push_back(message);
        std::cout << "[MockEmail] Sent to: " << message.to << " Subject: " << message.subject << std::endl;
        return true;
    }
    
    bool isConfigured() const override { return true; }
    
    void setSimulateFailure(bool fail) { simulate_failure_ = fail; }
    
    std::vector<EmailMessage> getSentEmails() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sent_emails_;
    }
    
    void clearSentEmails() {
        std::lock_guard<std::mutex> lock(mutex_);
        sent_emails_.clear();
    }
};

// ==================== SMTPé‚®ä»¶æœåŠ¡ ====================
class SMTPEmailService : public IEmailService {
private:
    SMTPConfig config_;
    
public:
    explicit SMTPEmailService(const SMTPConfig& config) : config_(config) {}
    
    bool send(const EmailMessage& message) override {
        // æž„å»ºMIMEæ¶ˆæ¯
        std::string mime_message = buildMimeMessage(message);
        
        // è¿™é‡Œæ˜¯ç®€åŒ–çš„SMTPå‘é€é€»è¾‘
        // å®žé™…å®žçŽ°éœ€è¦ä½¿ç”¨socketè¿žæŽ¥SMTPæœåŠ¡å™¨
        // æ”¯æŒTLS/SSL, AUTH LOGINç­‰
        
        std::cout << "[SMTP] Sending to: " << message.to << std::endl;
        std::cout << "[SMTP] Subject: " << message.subject << std::endl;
        
        // æ¨¡æ‹Ÿå‘é€æˆåŠŸ
        return true;
    }
    
    bool isConfigured() const override {
        return !config_.server.empty() && config_.port > 0;
    }
    
private:
    std::string buildMimeMessage(const EmailMessage& msg) {
        std::stringstream ss;
        
        // é‚®ä»¶å¤´
        ss << "From: " << config_.from_name << " <" << config_.from_address << ">\r\n";
        ss << "To: " << msg.to << "\r\n";
        ss << "Subject: " << msg.subject << "\r\n";
        ss << "MIME-Version: 1.0\r\n";
        
        if (!msg.body_html.empty()) {
            ss << "Content-Type: multipart/alternative; boundary=\"boundary123\"\r\n\r\n";
            ss << "--boundary123\r\n";
            ss << "Content-Type: text/plain; charset=utf-8\r\n\r\n";
            ss << msg.body_text << "\r\n";
            ss << "--boundary123\r\n";
            ss << "Content-Type: text/html; charset=utf-8\r\n\r\n";
            ss << msg.body_html << "\r\n";
            ss << "--boundary123--\r\n";
        } else {
            ss << "Content-Type: text/plain; charset=utf-8\r\n\r\n";
            ss << msg.body_text;
        }
        
        return ss.str();
    }
};

// ==================== é‚®ä»¶é€šçŸ¥ç®¡ç†å™¨ ====================
class EmailNotificationManager {
private:
    std::shared_ptr<IEmailService> email_service_;
    EmailTemplateManager template_manager_;
    
    std::queue<EmailMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> next_message_id_;
    
    // ç»Ÿè®¡ä¿¡æ¯
    std::atomic<uint64_t> total_sent_;
    std::atomic<uint64_t> total_failed_;
    
public:
    EmailNotificationManager(std::shared_ptr<IEmailService> service = nullptr)
        : email_service_(service), running_(false), next_message_id_(1),
          total_sent_(0), total_failed_(0) {
        if (!email_service_) {
            email_service_ = std::make_shared<MockEmailService>();
        }
    }
    
    ~EmailNotificationManager() {
        stop();
    }
    
    void start() {
        running_ = true;
        worker_thread_ = std::thread(&EmailNotificationManager::workerLoop, this);
    }
    
    void stop() {
        running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // å‘é€è®ºæ–‡æäº¤é€šçŸ¥
    void notifyPaperSubmitted(const std::string& author_email, const std::string& author_name,
                              uint32_t paper_id, const std::string& paper_title) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::PAPER_SUBMITTED);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["author_name"] = author_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["submit_time"] = getCurrentTimeString();
        
        auto msg = tmpl->createMessage(vars);
        msg.to = author_email;
        msg.priority = EmailPriority::NORMAL;
        
        queueMessage(msg);
    }
    
    // å‘é€å®¡ç¨¿åˆ†é…é€šçŸ¥
    void notifyReviewerAssigned(const std::string& reviewer_email, const std::string& reviewer_name,
                                uint32_t paper_id, const std::string& paper_title,
                                const std::string& research_area, const std::string& deadline) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::PAPER_ASSIGNED);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["reviewer_name"] = reviewer_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["research_area"] = research_area;
        vars["deadline"] = deadline;
        
        auto msg = tmpl->createMessage(vars);
        msg.to = reviewer_email;
        msg.priority = EmailPriority::HIGH;
        
        queueMessage(msg);
    }
    
    // å‘é€å®¡ç¨¿æé†’
    void notifyReviewReminder(const std::string& reviewer_email, const std::string& reviewer_name,
                              uint32_t paper_id, const std::string& paper_title,
                              const std::string& deadline, int days_remaining) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::REVIEW_REMINDER);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["reviewer_name"] = reviewer_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["deadline"] = deadline;
        vars["days_remaining"] = std::to_string(days_remaining);
        
        auto msg = tmpl->createMessage(vars);
        msg.to = reviewer_email;
        msg.priority = days_remaining <= 3 ? EmailPriority::URGENT : EmailPriority::HIGH;
        
        queueMessage(msg);
    }
    
    // å‘é€è®ºæ–‡æŽ¥å—é€šçŸ¥
    void notifyPaperAccepted(const std::string& author_email, const std::string& author_name,
                             uint32_t paper_id, const std::string& paper_title,
                             const std::string& editor_comments) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::PAPER_ACCEPTED);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["author_name"] = author_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["decision_date"] = getCurrentTimeString();
        vars["editor_comments"] = editor_comments;
        
        auto msg = tmpl->createMessage(vars);
        msg.to = author_email;
        msg.priority = EmailPriority::HIGH;
        
        queueMessage(msg);
    }
    
    // å‘é€è®ºæ–‡æ‹’ç»é€šçŸ¥
    void notifyPaperRejected(const std::string& author_email, const std::string& author_name,
                             uint32_t paper_id, const std::string& paper_title,
                             const std::string& editor_comments) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::PAPER_REJECTED);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["author_name"] = author_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["decision_date"] = getCurrentTimeString();
        vars["editor_comments"] = editor_comments;
        
        auto msg = tmpl->createMessage(vars);
        msg.to = author_email;
        msg.priority = EmailPriority::NORMAL;
        
        queueMessage(msg);
    }
    
    // å‘é€ä¿®è®¢è¦æ±‚é€šçŸ¥
    void notifyRevisionRequired(const std::string& author_email, const std::string& author_name,
                                uint32_t paper_id, const std::string& paper_title,
                                const std::string& revision_type, const std::string& revision_deadline,
                                const std::string& editor_comments, const std::string& reviewer_feedback) {
        auto tmpl = template_manager_.getTemplate(EmailTemplateType::REVISION_REQUIRED);
        if (!tmpl) return;
        
        std::map<std::string, std::string> vars;
        vars["author_name"] = author_name;
        vars["paper_id"] = std::to_string(paper_id);
        vars["paper_title"] = paper_title;
        vars["revision_type"] = revision_type;
        vars["revision_deadline"] = revision_deadline;
        vars["editor_comments"] = editor_comments;
        vars["reviewer_feedback"] = reviewer_feedback;
        
        auto msg = tmpl->createMessage(vars);
        msg.to = author_email;
        msg.priority = EmailPriority::HIGH;
        
        queueMessage(msg);
    }
    
    // ç›´æŽ¥å‘é€è‡ªå®šä¹‰é‚®ä»¶
    void sendCustomEmail(const std::string& to, const std::string& subject,
                         const std::string& body, const std::string& html_body = "") {
        EmailMessage msg;
        msg.to = to;
        msg.subject = subject;
        msg.body_text = body;
        msg.body_html = html_body;
        msg.priority = EmailPriority::NORMAL;
        
        queueMessage(msg);
    }
    
    // èŽ·å–ç»Ÿè®¡ä¿¡æ¯
    uint64_t getTotalSent() const { return total_sent_; }
    uint64_t getTotalFailed() const { return total_failed_; }
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return message_queue_.size();
    }
    
private:
    void queueMessage(EmailMessage& msg) {
        msg.message_id = next_message_id_++;
        msg.created_time = time(nullptr);
        msg.status = EmailStatus::PENDING;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(msg);
    }
    
    void workerLoop() {
        while (running_) {
            EmailMessage msg;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (message_queue_.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                msg = message_queue_.front();
                message_queue_.pop();
            }
            
            msg.status = EmailStatus::SENDING;
            
            if (email_service_ && email_service_->send(msg)) {
                msg.status = EmailStatus::SENT;
                msg.sent_time = time(nullptr);
                total_sent_++;
            } else {
                msg.status = EmailStatus::FAILED;
                msg.retry_count++;
                total_failed_++;
                
                // é‡è¯•é€»è¾‘
                if (msg.retry_count < 3) {
                    msg.status = EmailStatus::RETRYING;
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    message_queue_.push(msg);
                }
            }
        }
    }
    
    std::string getCurrentTimeString() {
        time_t now = time(nullptr);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string(buffer);
    }
};

#endif // EMAIL_NOTIFICATION_H