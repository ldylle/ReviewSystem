#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>

// ==================== ANSI颜色代码 ====================
namespace Color {
    // 基础颜色
    constexpr const char* RESET     = "\033[0m";
    constexpr const char* BLACK     = "\033[30m";
    constexpr const char* RED       = "\033[31m";
    constexpr const char* GREEN     = "\033[32m";
    constexpr const char* YELLOW    = "\033[33m";
    constexpr const char* BLUE      = "\033[34m";
    constexpr const char* MAGENTA   = "\033[35m";
    constexpr const char* CYAN      = "\033[36m";
    constexpr const char* WHITE     = "\033[37m";
    
    // 亮色
    constexpr const char* BRIGHT_BLACK   = "\033[90m";
    constexpr const char* BRIGHT_RED     = "\033[91m";
    constexpr const char* BRIGHT_GREEN   = "\033[92m";
    constexpr const char* BRIGHT_YELLOW  = "\033[93m";
    constexpr const char* BRIGHT_BLUE    = "\033[94m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
    constexpr const char* BRIGHT_CYAN    = "\033[96m";
    constexpr const char* BRIGHT_WHITE   = "\033[97m";
    
    // 背景色
    constexpr const char* BG_RED    = "\033[41m";
    constexpr const char* BG_GREEN  = "\033[42m";
    constexpr const char* BG_YELLOW = "\033[43m";
    constexpr const char* BG_BLUE   = "\033[44m";
    
    // 样式
    constexpr const char* BOLD      = "\033[1m";
    constexpr const char* DIM       = "\033[2m";
    constexpr const char* ITALIC    = "\033[3m";
    constexpr const char* UNDERLINE = "\033[4m";
    constexpr const char* BLINK     = "\033[5m";
    constexpr const char* REVERSE   = "\033[7m";
}

// ==================== 符号定义 ====================
namespace Symbol {
    // Unicode box drawing (UTF-8)
    constexpr const char* BOX_TL = "┌";  // top-left
    constexpr const char* BOX_TR = "┐";  // top-right
    constexpr const char* BOX_BL = "└";  // bottom-left
    constexpr const char* BOX_BR = "┘";  // bottom-right
    constexpr const char* BOX_H  = "─";  // horizontal
    constexpr const char* BOX_V  = "│";  // vertical
    constexpr const char* BOX_VL = "├";  // vertical-left
    constexpr const char* BOX_VR = "┤";  // vertical-right
    constexpr const char* BOX_HT = "┬";  // horizontal-top
    constexpr const char* BOX_HB = "┴";  // horizontal-bottom
    constexpr const char* BOX_X  = "┼";  // cross
    
    // Double line
    constexpr const char* DBOX_TL = "╔";
    constexpr const char* DBOX_TR = "╗";
    constexpr const char* DBOX_BL = "╚";
    constexpr const char* DBOX_BR = "╝";
    constexpr const char* DBOX_H  = "═";
    constexpr const char* DBOX_V  = "║";
    
    // 状态图标
    constexpr const char* CHECK    = "✓";
    constexpr const char* CROSS    = "✗";
    constexpr const char* WARNING  = "⚠";
    constexpr const char* INFO     = "ℹ";
    constexpr const char* STAR     = "★";
    constexpr const char* ARROW    = "→";
    constexpr const char* BULLET   = "•";
    constexpr const char* CIRCLE   = "○";
    constexpr const char* FILLED   = "●";
    
    // 进度条
    constexpr const char* PROGRESS_FULL  = "█";
    constexpr const char* PROGRESS_HALF  = "▌";
    constexpr const char* PROGRESS_EMPTY = "░";
}

// ==================== 终端UI工具类 ====================
class TerminalUI {
public:
    // ===== 基础格式化 =====
    static std::string colorize(const std::string& text, const char* color) {
        return std::string(color) + text + Color::RESET;
    }
    
    static std::string bold(const std::string& text) {
        return std::string(Color::BOLD) + text + Color::RESET;
    }
    
    static std::string underline(const std::string& text) {
        return std::string(Color::UNDERLINE) + text + Color::RESET;
    }
    
    // ===== 状态输出 =====
    static std::string success(const std::string& msg) {
        return std::string(Color::GREEN) + Symbol::CHECK + " " + msg + Color::RESET;
    }
    
    static std::string error(const std::string& msg) {
        return std::string(Color::RED) + Symbol::CROSS + " " + msg + Color::RESET;
    }
    
    static std::string warning(const std::string& msg) {
        return std::string(Color::YELLOW) + Symbol::WARNING + " " + msg + Color::RESET;
    }
    
    static std::string info(const std::string& msg) {
        return std::string(Color::CYAN) + Symbol::INFO + " " + msg + Color::RESET;
    }
    
    // ===== 进度条 =====
    static std::string progress_bar(double progress, int width = 30, 
                                   const std::string& label = "") {
        std::stringstream ss;
        
        int filled = static_cast<int>(progress * width);
        int empty = width - filled;
        
        ss << Color::CYAN << "[";
        ss << Color::GREEN;
        for (int i = 0; i < filled; ++i) ss << Symbol::PROGRESS_FULL;
        ss << Color::BRIGHT_BLACK;
        for (int i = 0; i < empty; ++i) ss << Symbol::PROGRESS_EMPTY;
        ss << Color::CYAN << "] ";
        ss << Color::WHITE << std::fixed << std::setprecision(1) << (progress * 100) << "%";
        
        if (!label.empty()) {
            ss << " " << label;
        }
        
        ss << Color::RESET;
        return ss.str();
    }
    
    // ===== 分隔线 =====
    static std::string divider(int width = 60, char c = '-') {
        return std::string(Color::BRIGHT_BLACK) + std::string(width, c) + Color::RESET;
    }
    
    static std::string double_divider(int width = 60) {
        std::string result = Color::CYAN;
        for (int i = 0; i < width; ++i) result += Symbol::DBOX_H;
        result += Color::RESET;
        return result;
    }
    
    // ===== 标题 =====
    static std::string title(const std::string& text, int width = 60) {
        std::stringstream ss;
        int padding = (width - text.length() - 2) / 2;
        
        ss << Color::CYAN << Color::BOLD;
        ss << Symbol::DBOX_TL;
        for (int i = 0; i < width - 2; ++i) ss << Symbol::DBOX_H;
        ss << Symbol::DBOX_TR << "\n";
        
        ss << Symbol::DBOX_V;
        ss << std::string(padding, ' ') << text;
        ss << std::string(width - 2 - padding - text.length(), ' ');
        ss << Symbol::DBOX_V << "\n";
        
        ss << Symbol::DBOX_BL;
        for (int i = 0; i < width - 2; ++i) ss << Symbol::DBOX_H;
        ss << Symbol::DBOX_BR;
        ss << Color::RESET;
        
        return ss.str();
    }
    
    // ===== 表格 =====
    struct TableColumn {
        std::string header;
        int width;
        bool align_right;
        
        TableColumn(const std::string& h, int w, bool right = false) 
            : header(h), width(w), align_right(right) {}
    };
    
    static std::string table_header(const std::vector<TableColumn>& columns) {
        std::stringstream ss;
        
        // 上边框
        ss << Color::CYAN << Symbol::BOX_TL;
        for (size_t i = 0; i < columns.size(); ++i) {
            for (int j = 0; j < columns[i].width + 2; ++j) ss << Symbol::BOX_H;
            ss << (i < columns.size() - 1 ? Symbol::BOX_HT : Symbol::BOX_TR);
        }
        ss << "\n";
        
        // 表头
        ss << Symbol::BOX_V;
        for (const auto& col : columns) {
            ss << " " << Color::BOLD << Color::WHITE;
            ss << std::setw(col.width) << (col.align_right ? std::right : std::left) << col.header;
            ss << Color::RESET << Color::CYAN << " " << Symbol::BOX_V;
        }
        ss << "\n";
        
        // 分隔线
        ss << Symbol::BOX_VL;
        for (size_t i = 0; i < columns.size(); ++i) {
            for (int j = 0; j < columns[i].width + 2; ++j) ss << Symbol::BOX_H;
            ss << (i < columns.size() - 1 ? Symbol::BOX_X : Symbol::BOX_VR);
        }
        ss << Color::RESET << "\n";
        
        return ss.str();
    }
    
    static std::string table_row(const std::vector<TableColumn>& columns,
                                const std::vector<std::string>& values,
                                const char* row_color = Color::WHITE) {
        std::stringstream ss;
        
        ss << Color::CYAN << Symbol::BOX_V;
        for (size_t i = 0; i < columns.size() && i < values.size(); ++i) {
            ss << " " << row_color;
            ss << std::setw(columns[i].width) 
               << (columns[i].align_right ? std::right : std::left) 
               << values[i].substr(0, columns[i].width);
            ss << Color::RESET << Color::CYAN << " " << Symbol::BOX_V;
        }
        ss << Color::RESET << "\n";
        
        return ss.str();
    }
    
    static std::string table_footer(const std::vector<TableColumn>& columns) {
        std::stringstream ss;
        
        ss << Color::CYAN << Symbol::BOX_BL;
        for (size_t i = 0; i < columns.size(); ++i) {
            for (int j = 0; j < columns[i].width + 2; ++j) ss << Symbol::BOX_H;
            ss << (i < columns.size() - 1 ? Symbol::BOX_HB : Symbol::BOX_BR);
        }
        ss << Color::RESET << "\n";
        
        return ss.str();
    }
    
    // ===== 状态徽章 =====
    static std::string status_badge(const std::string& status) {
        if (status == "SUBMITTED" || status == "1") {
            return colorize(" SUBMITTED ", Color::BG_BLUE) + " ";
        } else if (status == "UNDER_REVIEW" || status == "2") {
            return colorize(" REVIEWING ", Color::BG_YELLOW) + " ";
        } else if (status == "ACCEPTED" || status == "5") {
            return colorize(" ACCEPTED ", Color::BG_GREEN) + " ";
        } else if (status == "REJECTED" || status == "6") {
            return colorize(" REJECTED ", Color::BG_RED) + " ";
        } else if (status == "REVISION" || status == "3" || status == "4") {
            return colorize(" REVISION ", Color::YELLOW) + " ";
        }
        return status;
    }
    
    // ===== 角色徽章 =====
    static std::string role_badge(const std::string& role) {
        if (role == "ADMIN" || role == "1") {
            return std::string(Color::BG_RED) + Color::WHITE + " ADMIN " + Color::RESET;
        } else if (role == "EDITOR" || role == "2") {
            return std::string(Color::BG_BLUE) + Color::WHITE + " EDITOR " + Color::RESET;
        } else if (role == "REVIEWER" || role == "3") {
            return std::string(Color::BG_GREEN) + Color::WHITE + " REVIEWER " + Color::RESET;
        } else if (role == "AUTHOR" || role == "4") {
            return std::string(Color::BG_YELLOW) + Color::BLACK + " AUTHOR " + Color::RESET;
        }
        return role;
    }
    
    // ===== 时间格式化 =====
    static std::string format_time(time_t t) {
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", localtime(&t));
        return std::string(buffer);
    }
    
    static std::string relative_time(time_t t) {
        time_t now = time(nullptr);
        int diff = now - t;
        
        if (diff < 60) return "just now";
        if (diff < 3600) return std::to_string(diff / 60) + " min ago";
        if (diff < 86400) return std::to_string(diff / 3600) + " hours ago";
        if (diff < 604800) return std::to_string(diff / 86400) + " days ago";
        
        return format_time(t);
    }
    
    // ===== 清屏 =====
    static void clear_screen() {
        std::cout << "\033[2J\033[H";
    }
    
    // ===== 光标控制 =====
    static void cursor_up(int n = 1) {
        std::cout << "\033[" << n << "A";
    }
    
    static void cursor_down(int n = 1) {
        std::cout << "\033[" << n << "B";
    }
    
    static void cursor_forward(int n = 1) {
        std::cout << "\033[" << n << "C";
    }
    
    static void cursor_back(int n = 1) {
        std::cout << "\033[" << n << "D";
    }
    
    // ===== Logo/Banner =====
    static std::string banner() {
        std::stringstream ss;
        ss << Color::CYAN << Color::BOLD;
        ss << R"(
  ╦═╗╔═╗╦  ╦╦╔═╗╦ ╦  ╔═╗╦ ╦╔═╗╔╦╗╔═╗╔╦╗
  ╠╦╝║╣ ╚╗╔╝║║╣ ║║║  ╚═╗╚╦╝╚═╗ ║ ║╣ ║║║
  ╩╚═╚═╝ ╚╝ ╩╚═╝╚╩╝  ╚═╝ ╩ ╚═╝ ╩ ╚═╝╩ ╩
)" << Color::RESET;
        ss << Color::BRIGHT_BLACK << "        Scientific Paper Review System v2.0\n" << Color::RESET;
        return ss.str();
    }
    
    // ===== 帮助框 =====
    static std::string help_box(const std::string& title, 
                               const std::vector<std::pair<std::string, std::string>>& items) {
        std::stringstream ss;
        
        ss << Color::CYAN << "\n  " << title << "\n";
        ss << Color::BRIGHT_BLACK << "  " << std::string(50, '-') << "\n";
        
        for (const auto& [cmd, desc] : items) {
            ss << "  " << Color::YELLOW << std::setw(20) << std::left << cmd;
            ss << Color::WHITE << desc << "\n";
        }
        
        ss << Color::RESET << "\n";
        return ss.str();
    }
};

// ==================== 系统统计仪表板 ====================
class Dashboard {
public:
    static std::string render(uint64_t total_blocks, uint64_t free_blocks,
                             uint64_t total_inodes, uint64_t free_inodes,
                             double cache_hit_rate,
                             uint32_t active_users, uint32_t total_papers) {
        std::stringstream ss;
        
        ss << TerminalUI::title("SYSTEM DASHBOARD") << "\n\n";
        
        // 存储使用
        double block_usage = 1.0 - static_cast<double>(free_blocks) / total_blocks;
        double inode_usage = 1.0 - static_cast<double>(free_inodes) / total_inodes;
        
        ss << Color::WHITE << "  Storage Usage:\n";
        ss << "    Blocks: " << TerminalUI::progress_bar(block_usage, 25) 
           << " (" << free_blocks << "/" << total_blocks << " free)\n";
        ss << "    Inodes: " << TerminalUI::progress_bar(inode_usage, 25)
           << " (" << free_inodes << "/" << total_inodes << " free)\n\n";
        
        // 缓存性能
        ss << "  Cache Performance:\n";
        ss << "    Hit Rate: " << TerminalUI::progress_bar(cache_hit_rate, 25);
        if (cache_hit_rate > 0.8) {
            ss << Color::GREEN << " Excellent" << Color::RESET;
        } else if (cache_hit_rate > 0.5) {
            ss << Color::YELLOW << " Good" << Color::RESET;
        } else {
            ss << Color::RED << " Needs Improvement" << Color::RESET;
        }
        ss << "\n\n";
        
        // 系统统计
        ss << "  System Stats:\n";
        ss << "    " << Color::CYAN << Symbol::BULLET << Color::WHITE 
           << " Active Users: " << Color::YELLOW << active_users << Color::RESET << "\n";
        ss << "    " << Color::CYAN << Symbol::BULLET << Color::WHITE
           << " Total Papers: " << Color::YELLOW << total_papers << Color::RESET << "\n";
        
        ss << "\n" << TerminalUI::double_divider() << "\n";
        
        return ss.str();
    }
};

// ==================== 论文状态时间线 ====================
class PaperTimeline {
public:
    struct TimelineEvent {
        time_t timestamp;
        std::string event;
        std::string actor;
        std::string details;
    };
    
    static std::string render(const std::vector<TimelineEvent>& events) {
        std::stringstream ss;
        
        ss << Color::CYAN << Color::BOLD << "\n  Paper Timeline\n" << Color::RESET;
        
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            bool is_last = (i == events.size() - 1);
            
            // 时间
            ss << Color::BRIGHT_BLACK << "  " << TerminalUI::format_time(e.timestamp) << "  ";
            
            // 连接线
            if (is_last) {
                ss << Color::CYAN << "└" << Symbol::BOX_H << Symbol::BOX_H;
            } else {
                ss << Color::CYAN << "├" << Symbol::BOX_H << Symbol::BOX_H;
            }
            
            // 事件图标
            if (e.event.find("Submit") != std::string::npos) {
                ss << Color::GREEN << " " << Symbol::FILLED << " ";
            } else if (e.event.find("Review") != std::string::npos) {
                ss << Color::YELLOW << " " << Symbol::FILLED << " ";
            } else if (e.event.find("Accept") != std::string::npos) {
                ss << Color::GREEN << " " << Symbol::STAR << " ";
            } else if (e.event.find("Reject") != std::string::npos) {
                ss << Color::RED << " " << Symbol::CROSS << " ";
            } else {
                ss << Color::BLUE << " " << Symbol::CIRCLE << " ";
            }
            
            // 事件内容
            ss << Color::WHITE << e.event;
            if (!e.actor.empty()) {
                ss << Color::BRIGHT_BLACK << " by " << e.actor;
            }
            ss << Color::RESET << "\n";
            
            // 详情
            if (!e.details.empty()) {
                ss << "                    ";
                if (!is_last) ss << Color::CYAN << "│" << Color::RESET;
                else ss << " ";
                ss << "   " << Color::BRIGHT_BLACK << e.details << Color::RESET << "\n";
            }
        }
        
        ss << "\n";
        return ss.str();
    }
};

#endif // TERMINAL_UI_H
