#include "protocol.h"
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <iostream>

// ==================== Base64编码/解码 ====================
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string SubmitPaperRequest::base64_encode(const std::vector<uint8_t>& data) {
    std::string ret;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];
    size_t in_len = data.size();
    const uint8_t* bytes_to_encode = data.data();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::vector<uint8_t> SubmitPaperRequest::base64_decode(const std::string& encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && (encoded_string[in_] != '=') && 
           (isalnum(encoded_string[in_]) || (encoded_string[in_] == '+') || (encoded_string[in_] == '/'))) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; j < i - 1; j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
}

// ==================== MessageHeader实现 ====================
std::vector<uint8_t> MessageHeader::serialize() const {
    std::vector<uint8_t> data(sizeof(MessageHeader));
    memcpy(data.data(), this, sizeof(MessageHeader));
    return data;
}

MessageHeader MessageHeader::deserialize(const std::vector<uint8_t>& data) {
    MessageHeader header;
    if (data.size() >= sizeof(MessageHeader)) {
        memcpy(&header, data.data(), sizeof(MessageHeader));
    }
    return header;
}

// ==================== Message实现 ====================
// [最终修复版] 去掉 const，并调用 build_payload 同步数据
std::vector<uint8_t> Message::serialize() {
    // 关键步骤：同步数据到 JSON
    build_payload();

    std::string payload_str = payload_.dump();
    
    MessageHeader header = header_;
    header.payload_size = payload_str.size();
    header.timestamp = time(nullptr);
    
    std::vector<uint8_t> data;
    auto header_data = header.serialize();
    data.insert(data.end(), header_data.begin(), header_data.end());
    data.insert(data.end(), payload_str.begin(), payload_str.end());
    
    return data;
}

std::unique_ptr<Message> Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        return nullptr;
    }
    
    MessageHeader header = MessageHeader::deserialize(data);
    
    if (header.magic != PROTOCOL_MAGIC) {
        return nullptr;
    }
    
    if (data.size() < sizeof(MessageHeader) + header.payload_size) {
        return nullptr;
    }
    
    std::string payload_str(
        data.begin() + sizeof(MessageHeader),
        data.begin() + sizeof(MessageHeader) + header.payload_size
    );
    
    auto msg = std::make_unique<Message>(header.type);
    msg->header_ = header;
    
    try {
        msg->payload_ = json::parse(payload_str);
    } catch (...) {
        return nullptr;
    }
    
    return msg;
}

// ==================== 工具函数实现 ====================
namespace ProtocolUtils {
    std::string role_to_string(UserRole role) {
        switch (role) {
            case UserRole::ADMIN: return "ADMIN";
            case UserRole::EDITOR: return "EDITOR";
            case UserRole::REVIEWER: return "REVIEWER";
            case UserRole::AUTHOR: return "AUTHOR";
            default: return "UNKNOWN";
        }
    }
    
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
    
    std::string generate_session_token() {
        // [关键修复] 使用时间种子防止虚拟机卡死
        static std::mt19937 gen(static_cast<unsigned int>(time(nullptr)));
        
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << std::hex;
        
        for (int i = 0; i < 32; i++) {
            ss << dis(gen);
            if (i == 7 || i == 11 || i == 15 || i == 19)
                ss << "-";
        }
        
        return ss.str();
    }
    
    bool validate_session_token(const std::string& token) {
        return token.length() > 0 && token.length() < 256;
    }
}