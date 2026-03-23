#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <sstream>
#include <fstream>
#include <regex>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// JSONÃ¥Âºâ€œ
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ==================== HTTPÃ¨Â¯Â·Ã¦Â±â€š ====================
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::map<std::string, std::string> cookies;
    std::string body;
    std::string client_ip;
    
    // Ã¨Â§Â£Ã¦Å¾ÂÃ¦Å¸Â¥Ã¨Â¯Â¢Ã¥Ââ€šÃ¦â€¢Â°
    void parseQueryString() {
        if (query_string.empty()) return;
        
        std::istringstream iss(query_string);
        std::string pair;
        
        while (std::getline(iss, pair, '&')) {
            size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = urlDecode(pair.substr(0, pos));
                std::string value = urlDecode(pair.substr(pos + 1));
                params[key] = value;
            }
        }
    }
    
    // Ã¨Â§Â£Ã¦Å¾ÂCookie
    void parseCookies() {
        auto it = headers.find("Cookie");
        if (it == headers.end()) return;
        
        std::istringstream iss(it->second);
        std::string pair;
        
        while (std::getline(iss, pair, ';')) {
            // Ã¥Å½Â»Ã©â„¢Â¤Ã¥â€°ÂÃ¥Â¯Â¼Ã§Â©ÂºÃ¦Â Â¼
            size_t start = pair.find_first_not_of(" ");
            if (start != std::string::npos) {
                pair = pair.substr(start);
            }
            
            size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                cookies[pair.substr(0, pos)] = pair.substr(pos + 1);
            }
        }
    }
    
    // Ã¨Å½Â·Ã¥Ââ€“JSON body
    json getJson() const {
        try {
            return json::parse(body);
        } catch (...) {
            return json::object();
        }
    }
    
private:
    static std::string urlDecode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == '%' && i + 2 < str.size()) {
                int value;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += str[i];
                }
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }
};

// ==================== HTTPÃ¥â€œÂÃ¥Âºâ€ ====================
struct HttpResponse {
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse() : status_code(200), status_text("OK") {
        headers["Content-Type"] = "text/html; charset=utf-8";
        headers["Server"] = "ReviewSystem/1.0";
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®Ã§Å Â¶Ã¦â‚¬Â
    HttpResponse& setStatus(int code, const std::string& text = "") {
        status_code = code;
        status_text = text.empty() ? getDefaultStatusText(code) : text;
        return *this;
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®Ã¥Â¤Â´Ã©Æ’Â¨
    HttpResponse& setHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
        return *this;
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®Cookie
    HttpResponse& setCookie(const std::string& name, const std::string& value,
                           int max_age = 86400, const std::string& path = "/") {
        std::stringstream ss;
        ss << name << "=" << value << "; Max-Age=" << max_age << "; Path=" << path << "; HttpOnly";
        headers["Set-Cookie"] = ss.str();
        return *this;
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®HTMLÃ¥â€ â€¦Ã¥Â®Â¹
    HttpResponse& html(const std::string& content) {
        headers["Content-Type"] = "text/html; charset=utf-8";
        body = content;
        return *this;
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®JSONÃ¥â€ â€¦Ã¥Â®Â¹
    HttpResponse& json(const ::json& j) {
        headers["Content-Type"] = "application/json";
        body = j.dump();
        return *this;
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®Ã¦â€“â€¡Ã¤Â»Â¶Ã¥â€ â€¦Ã¥Â®Â¹
    HttpResponse& file(const std::string& filepath, const std::string& mime_type = "") {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return setStatus(404).html("File not found");
        }
        
        std::stringstream ss;
        ss << file.rdbuf();
        body = ss.str();
        
        if (!mime_type.empty()) {
            headers["Content-Type"] = mime_type;
        } else {
            headers["Content-Type"] = guessMimeType(filepath);
        }
        
        return *this;
    }
    
    // Ã©â€¡ÂÃ¥Â®Å¡Ã¥Ââ€˜
    HttpResponse& redirect(const std::string& url) {
        status_code = 302;
        status_text = "Found";
        headers["Location"] = url;
        return *this;
    }
    
    // Ã¥ÂºÂÃ¥Ë†â€”Ã¥Å’â€“Ã¤Â¸ÂºHTTPÃ¥â€œÂÃ¥Âºâ€Ã¥Â­â€”Ã§Â¬Â¦Ã¤Â¸Â²
    std::string serialize() const {
        std::stringstream ss;
        ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        
        for (const auto& [key, value] : headers) {
            ss << key << ": " << value << "\r\n";
        }
        
        ss << "Content-Length: " << body.size() << "\r\n";
        ss << "\r\n";
        ss << body;
        
        return ss.str();
    }
    
private:
    static std::string getDefaultStatusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }
    
    static std::string guessMimeType(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot == std::string::npos) return "application/octet-stream";
        
        std::string ext = path.substr(dot + 1);
        
        if (ext == "html" || ext == "htm") return "text/html";
        if (ext == "css") return "text/css";
        if (ext == "js") return "application/javascript";
        if (ext == "json") return "application/json";
        if (ext == "png") return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "gif") return "image/gif";
        if (ext == "svg") return "image/svg+xml";
        if (ext == "ico") return "image/x-icon";
        if (ext == "pdf") return "application/pdf";
        
        return "application/octet-stream";
    }
};

// ==================== Ã¨Â·Â¯Ã§â€Â±Ã¥Â¤â€žÃ§Ââ€ Ã¥â„¢Â¨Ã§Â±Â»Ã¥Å¾â€¹ ====================
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

// ==================== Ã¨Â·Â¯Ã§â€Â±Ã¥Â®Å¡Ã¤Â¹â€° ====================
struct Route {
    std::string method;
    std::regex pattern;
    std::string pattern_str;
    std::vector<std::string> param_names;
    RouteHandler handler;
    
    Route(const std::string& m, const std::string& p, RouteHandler h)
        : method(m), pattern_str(p), handler(h) {
        // Ã¥Â°â€ Ã¨Â·Â¯Ã§â€Â±Ã¦Â¨Â¡Ã¥Â¼ÂÃ¨Â½Â¬Ã¦ÂÂ¢Ã¤Â¸ÂºÃ¦Â­Â£Ã¥Ë†â„¢Ã¨Â¡Â¨Ã¨Â¾Â¾Ã¥Â¼Â
        std::string regex_str = "^" + p + "$";
        
        // Ã¦ÂÂÃ¥Ââ€“Ã¥Ââ€šÃ¦â€¢Â°Ã¥ÂÂ
        std::regex param_regex(":([a-zA-Z_][a-zA-Z0-9_]*)");
        std::sregex_iterator iter(p.begin(), p.end(), param_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            param_names.push_back((*iter)[1].str());
        }
        
        // Ã¦â€ºÂ¿Ã¦ÂÂ¢Ã¥Ââ€šÃ¦â€¢Â°Ã¤Â¸ÂºÃ¦Ââ€¢Ã¨Å½Â·Ã§Â»â€ž
        regex_str = std::regex_replace(regex_str, param_regex, "([^/]+)");
        
        pattern = std::regex(regex_str);
    }
    
    bool match(const std::string& m, const std::string& path, 
               std::map<std::string, std::string>& params) const {
        if (method != m && method != "*") return false;
        
        std::smatch match;
        if (std::regex_match(path, match, pattern)) {
            for (size_t i = 0; i < param_names.size(); i++) {
                params[param_names[i]] = match[i + 1].str();
            }
            return true;
        }
        
        return false;
    }
};

// ==================== Ã¤Â¸Â­Ã©â€”Â´Ã¤Â»Â¶Ã§Â±Â»Ã¥Å¾â€¹ ====================
using Middleware = std::function<bool(HttpRequest&, HttpResponse&)>;

// ==================== WebÃ¦Å“ÂÃ¥Å Â¡Ã¥â„¢Â¨ ====================
class WebServer {
private:
    int port_;
    int server_fd_;
    std::atomic<bool> running_;
    std::vector<Route> routes_;
    std::vector<Middleware> middlewares_;
    std::vector<std::thread> workers_;
    std::string static_dir_;
    mutable std::mutex routes_mutex_;
    
public:
    explicit WebServer(int port = 8080) 
        : port_(port), server_fd_(-1), running_(false), static_dir_("./static") {}
    
    ~WebServer() {
        stop();
    }
    
    // Ã¨Â®Â¾Ã§Â½Â®Ã©Ââ„¢Ã¦â‚¬ÂÃ¦â€“â€¡Ã¤Â»Â¶Ã§â€ºÂ®Ã¥Â½â€¢
    void setStaticDir(const std::string& dir) {
        static_dir_ = dir;
    }
    
    // Ã¦Â·Â»Ã¥Å Â Ã¤Â¸Â­Ã©â€”Â´Ã¤Â»Â¶
    void use(Middleware middleware) {
        middlewares_.push_back(middleware);
    }
    
    // Ã¦Â·Â»Ã¥Å Â Ã¨Â·Â¯Ã§â€Â±
    void route(const std::string& method, const std::string& path, RouteHandler handler) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_.emplace_back(method, path, handler);
    }
    
    // Ã¤Â¾Â¿Ã¦ÂÂ·Ã¦â€“Â¹Ã¦Â³â€¢
    void get(const std::string& path, RouteHandler handler) {
        route("GET", path, handler);
    }
    
    void post(const std::string& path, RouteHandler handler) {
        route("POST", path, handler);
    }
    
    void put(const std::string& path, RouteHandler handler) {
        route("PUT", path, handler);
    }
    
    void del(const std::string& path, RouteHandler handler) {
        route("DELETE", path, handler);
    }
    
    // Ã¥ÂÂ¯Ã¥Å Â¨Ã¦Å“ÂÃ¥Å Â¡Ã¥â„¢Â¨
    bool start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            std::cerr << "[WebServer] Failed to create socket" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "[WebServer] Failed to bind port " << port_ << std::endl;
            close(server_fd_);
            return false;
        }
        
        if (listen(server_fd_, 100) < 0) {
            std::cerr << "[WebServer] Failed to listen" << std::endl;
            close(server_fd_);
            return false;
        }
        
        running_ = true;
        std::cout << "[WebServer] Started on port " << port_ << std::endl;
        std::cout << "[WebServer] Access at: http://localhost:" << port_ << "/" << std::endl;
        
        return true;
    }
    
    // Ã¨Â¿ÂÃ¨Â¡Å’Ã¦Å“ÂÃ¥Å Â¡Ã¥â„¢Â¨(Ã©ËœÂ»Ã¥Â¡Å¾)
    void run() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (running_) {
                    std::cerr << "[WebServer] Accept failed" << std::endl;
                }
                continue;
            }
            
            // Ã¨Å½Â·Ã¥Ââ€“Ã¥Â®Â¢Ã¦Ë†Â·Ã§Â«Â¯IP
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            
            // Ã¥Å“Â¨Ã¦â€“Â°Ã§ÂºÂ¿Ã§Â¨â€¹Ã¤Â¸Â­Ã¥Â¤â€žÃ§Ââ€ Ã¨Â¯Â·Ã¦Â±â€š
            workers_.emplace_back(&WebServer::handleClient, this, client_fd, client_ip);
        }
    }
    
    // Ã¥ÂÅ“Ã¦Â­Â¢Ã¦Å“ÂÃ¥Å Â¡Ã¥â„¢Â¨
    void stop() {
        running_ = false;
        
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        
        std::cout << "[WebServer] Stopped" << std::endl;
    }
    

private:
    void handleClient(int client_fd, const std::string& client_ip) {
        // 使用动态缓冲区策略
        std::string request_data;
        std::vector<char> buffer(8192); // 8KB 块大小
        
        // 1. 循环读取直到获取到完整的头部 (double CRLF)
        while (request_data.find("\r\n\r\n") == std::string::npos) {
            ssize_t bytes_read = recv(client_fd, buffer.data(), buffer.size(), 0);
            if (bytes_read <= 0) {
                close(client_fd);
                return;
            }
            request_data.append(buffer.data(), bytes_read);
        }
        
        // 2. 解析 Content-Length 以确定剩余数据量
        size_t header_end_pos = request_data.find("\r\n\r\n");
        size_t content_length = 0;
        
        // 在头部中查找 Content-Length (忽略大小写)
        std::string headers_part = request_data.substr(0, header_end_pos);
        // 转小写以便查找
        std::string headers_lower = headers_part;
        std::transform(headers_lower.begin(), headers_lower.end(), headers_lower.begin(), ::tolower);
        
        size_t cl_pos = headers_lower.find("content-length:");
        if (cl_pos != std::string::npos) {
            // 找到 content-length: 后的数值
            size_t val_start = cl_pos + 15; // "content-length:".length()
            size_t val_end = headers_lower.find("\r\n", val_start);
            if (val_end != std::string::npos) {
                try {
                    std::string len_str = headers_lower.substr(val_start, val_end - val_start);
                    // 去除可能的空格
                    len_str.erase(0, len_str.find_first_not_of(" "));
                    content_length = std::stoul(len_str);
                } catch (...) {}
            }
        }
        
        // 3. 循环读取剩余的 Body 数据
        size_t total_expected_size = header_end_pos + 4 + content_length;
        
        while (request_data.size() < total_expected_size) {
            ssize_t bytes_read = recv(client_fd, buffer.data(), buffer.size(), 0);
            if (bytes_read <= 0) break;
            request_data.append(buffer.data(), bytes_read);
        }
        
        // 构造请求对象
        HttpRequest request = parseRequest(request_data);
        request.client_ip = client_ip;
        
        // 构造响应
        HttpResponse response;
        
        // 执行中间件
        bool proceed = true;
        for (auto& middleware : middlewares_) {
            if (!middleware(request, response)) {
                proceed = false;
                break;
            }
        }
        
        if (proceed) {
            // 路由匹配
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(routes_mutex_);
                for (const auto& route : routes_) {
                    if (route.match(request.method, request.path, request.params)) {
                        try {
                            response = route.handler(request);
                        } catch (const std::exception& e) {
                            response.setStatus(500).html("Internal Server Error: " + std::string(e.what()));
                        }
                        found = true;
                        break;
                    }
                }
            }
            
            // 尝试静态文件
            if (!found) {
                response = serveStatic(request.path);
            }
        }
        
        // 发送响应
        std::string response_str = response.serialize();
        // 循环发送以确保大响应也能发完
        size_t total_sent = 0;
        while (total_sent < response_str.size()) {
            ssize_t sent = send(client_fd, response_str.c_str() + total_sent, response_str.size() - total_sent, 0);
            if (sent <= 0) break;
            total_sent += sent;
        }
        
        close(client_fd);
        
        // 日志
        std::cout << "[WebServer] " << client_ip << " " << request.method << " " 
                  << request.path << " -> " << response.status_code 
                  << " (Body size: " << request.body.size() << ")" << std::endl;
    }
HttpRequest parseRequest(const std::string& raw) {
        HttpRequest request;
        std::istringstream iss(raw);
        std::string line;
        
        // 解析请求行
        if (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream line_iss(line);
            line_iss >> request.method >> request.path;
            
            size_t query_pos = request.path.find('?');
            if (query_pos != std::string::npos) {
                request.query_string = request.path.substr(query_pos + 1);
                request.path = request.path.substr(0, query_pos);
            }
        }
        
        // 解析头部
        while (std::getline(iss, line) && line != "\r" && !line.empty()) {
            if (line.back() == '\r') line.pop_back();
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                
                size_t start = value.find_first_not_of(" ");
                if (start != std::string::npos) value = value.substr(start);
                
                request.headers[key] = value;
            }
        }
        
        // 读取body - 增强兼容性
        size_t content_length = 0;
        bool length_found = false;
        
        // 尝试标准写法
        auto it = request.headers.find("Content-Length");
        if (it != request.headers.end()) {
            content_length = std::stoul(it->second);
            length_found = true;
        } 
        // 尝试小写写法
        else {
             it = request.headers.find("content-length");
             if (it != request.headers.end()) {
                 content_length = std::stoul(it->second);
                 length_found = true;
             }
        }
        
        if (length_found) {
            size_t body_start = raw.find("\r\n\r\n");
            if (body_start != std::string::npos && body_start + 4 < raw.size()) {
                // 安全截取，避免越界
                request.body = raw.substr(body_start + 4, content_length);
            }
        }
        
        request.parseQueryString();
        request.parseCookies();
        
        return request;
    }
    
    HttpResponse serveStatic(const std::string& path) {
        HttpResponse response;
        
        std::string file_path = static_dir_ + path;
        
        // Ã¥Â®â€°Ã¥â€¦Â¨Ã¦Â£â‚¬Ã¦Å¸Â¥ - Ã©ËœÂ²Ã¦Â­Â¢Ã§â€ºÂ®Ã¥Â½â€¢Ã©ÂÂÃ¥Å½â€ 
        if (file_path.find("..") != std::string::npos) {
            return response.setStatus(403).html("Forbidden");
        }
        
        // Ã©Â»ËœÃ¨Â®Â¤index.html
        if (path == "/") {
            file_path = static_dir_ + "/index.html";
        }
        
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return response.setStatus(404).html(generate404Page(path));
        }
        
        return response.file(file_path);
    }
    
    std::string generate404Page(const std::string& path) {
        std::stringstream ss;
        ss << R"(<!DOCTYPE html>
<html>
<head>
    <title>404 Not Found</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding: 50px; background: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 40px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #e74c3c; font-size: 72px; margin: 0; }
        p { color: #666; }
        a { color: #3498db; text-decoration: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>404</h1>
        <p>Page not found: )" << path << R"(</p>
        <p><a href="/">Return to Home</a></p>
    </div>
</body>
</html>)";
        return ss.str();
    }
};

// ==================== APIÃ¦Å½Â§Ã¥Ë†Â¶Ã¥â„¢Â¨Ã¥Å¸ÂºÃ§Â±Â» ====================
class ApiController {
protected:
    // Ã¦Ë†ÂÃ¥Å Å¸Ã¥â€œÂÃ¥Âºâ€
    HttpResponse success(const json& data, const std::string& message = "Success") {
        HttpResponse response;
        json result;
        if (data.is_object()) {
            result = data;
        } else {
            result["data"] = data;
        }

        result["success"] = true;
        if (!result.contains("message")) {
            result["message"] = message;
        }
        return response.json(result);
    }
    
    // Ã©â€â„¢Ã¨Â¯Â¯Ã¥â€œÂÃ¥Âºâ€
    HttpResponse error(int code, const std::string& message) {
        HttpResponse response;
        json result;
        result["success"] = false;
        result["error"] = code;
        result["message"] = message;
        return response.setStatus(code).json(result);
    }
    
    // Ã¥Ë†â€ Ã©Â¡ÂµÃ¥â€œÂÃ¥Âºâ€
    HttpResponse paginated(const json& items, int page, int per_page, int total) {
        HttpResponse response;
        json result;
        result["success"] = true;
        result["data"] = items;
        result["pagination"]["page"] = page;
        result["pagination"]["per_page"] = per_page;
        result["pagination"]["total"] = total;
        result["pagination"]["total_pages"] = (total + per_page - 1) / per_page;
        return response.json(result);
    }
};

// ==================== Ã¨Â®Â¤Ã¨Â¯ÂÃ¤Â¸Â­Ã©â€”Â´Ã¤Â»Â¶ ====================
class AuthMiddleware {
private:
    std::set<std::string> public_paths_;
    std::function<bool(const std::string&)> token_validator_;
    
public:
    AuthMiddleware() = default;
    
    void addPublicPath(const std::string& path) {
        public_paths_.insert(path);
    }
    
    void setTokenValidator(std::function<bool(const std::string&)> validator) {
        token_validator_ = validator;
    }
    
    Middleware getMiddleware() {
        return [this](HttpRequest& req, HttpResponse& res) -> bool {
            // Ã¦Â£â‚¬Ã¦Å¸Â¥Ã¦ËœÂ¯Ã¥ÂÂ¦Ã¦ËœÂ¯Ã¥â€¦Â¬Ã¥Â¼â‚¬Ã¨Â·Â¯Ã¥Â¾â€ž
            for (const auto& path : public_paths_) {
                if (req.path.find(path) == 0) {
                    return true;
                }
            }
            
            // Ã¦Â£â‚¬Ã¦Å¸Â¥token
            auto it = req.cookies.find("session_token");
            if (it == req.cookies.end()) {
                it = req.headers.find("Authorization");
                if (it == req.headers.end()) {
                    res.setStatus(401);
                    json err;
                    err["success"] = false;
                    err["message"] = "Unauthorized";
                    res.json(err);
                    return false;
                }
            }
            
            std::string token = it->second;
            
            // Bearer tokenÃ¥Â¤â€žÃ§Ââ€ 
            if (token.substr(0, 7) == "Bearer ") {
                token = token.substr(7);
            }
            
            if (token_validator_ && !token_validator_(token)) {
                res.setStatus(401);
                json err;
                err["success"] = false;
                err["message"] = "Invalid or expired token";
                res.json(err);
                return false;
            }
            
            req.params["session_token"] = token;
            return true;
        };
    }
};

// ==================== CORSÃ¤Â¸Â­Ã©â€”Â´Ã¤Â»Â¶ ====================
Middleware corsMiddleware(const std::string& allowed_origins = "*") {
    return [allowed_origins](HttpRequest& req, HttpResponse& res) -> bool {
        res.setHeader("Access-Control-Allow-Origin", allowed_origins);
        res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.setHeader("Access-Control-Allow-Credentials", "true");
        
        // Ã¥Â¤â€žÃ§Ââ€ Ã©Â¢â€žÃ¦Â£â‚¬Ã¨Â¯Â·Ã¦Â±â€š
        if (req.method == "OPTIONS") {
            res.setStatus(204);
            return false;  // Ã§â€ºÂ´Ã¦Å½Â¥Ã¨Â¿â€Ã¥â€ºÅ¾,Ã¤Â¸ÂÃ§Â»Â§Ã§Â»Â­Ã¥Â¤â€žÃ§Ââ€ 
        }
        
        return true;
    };
}

// ==================== Ã¦â€”Â¥Ã¥Â¿â€”Ã¤Â¸Â­Ã©â€”Â´Ã¤Â»Â¶ ====================
Middleware loggingMiddleware() {
    return [](HttpRequest& req, HttpResponse& res) -> bool {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
                  << req.client_ip << " " << req.method << " " << req.path << std::endl;
        
        return true;
    };
}

#endif // WEB_SERVER_H