#ifndef PDF_ANALYZER_H
#define PDF_ANALYZER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <numeric>

// ==================== PDF解析结果 ====================
struct PDFTextBlock {
    std::string text;
    int page_number;
    double x, y, width, height;  // 位置信息
    std::string font_name;
    double font_size;
    bool is_bold;
    bool is_italic;
    
    PDFTextBlock() : page_number(0), x(0), y(0), width(0), height(0),
                     font_size(12), is_bold(false), is_italic(false) {}
};

struct PDFDocument {
    std::string filename;
    std::string title;
    std::string author;
    std::string subject;
    std::string creator;
    time_t creation_date;
    time_t modification_date;
    int page_count;
    
    std::string full_text;                      // 完整文本
    std::vector<std::string> pages;             // 按页分割的文本
    std::vector<PDFTextBlock> text_blocks;      // 详细文本块
    
    // 结构化内容
    std::string abstract;
    std::vector<std::string> sections;
    std::vector<std::string> references;
    std::vector<std::string> figures;
    std::vector<std::string> tables;
    
    PDFDocument() : creation_date(0), modification_date(0), page_count(0) {}
};

// ==================== PDF解析器 ====================
class PDFParser {
public:
    virtual ~PDFParser() = default;
    
    // 从文件解析
    virtual bool parseFile(const std::string& filepath, PDFDocument& doc) = 0;
    
    // 从内存解析
    virtual bool parseMemory(const std::vector<uint8_t>& data, PDFDocument& doc) = 0;
    
    // 提取纯文本
    virtual std::string extractText(const std::vector<uint8_t>& data) = 0;
};

// ==================== 简化PDF解析器(基于文本流提取) ====================
class SimplePDFParser : public PDFParser {
public:
    bool parseFile(const std::string& filepath, PDFDocument& doc) override {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) return false;
        
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        return parseMemory(data, doc);
    }
    
    bool parseMemory(const std::vector<uint8_t>& data, PDFDocument& doc) override {
        doc.full_text = extractText(data);
        doc.filename = "memory_document";
        
        // 解析元数据
        extractMetadata(data, doc);
        
        // 解析结构
        extractStructure(doc);
        
        return !doc.full_text.empty();
    }
    
    std::string extractText(const std::vector<uint8_t>& data) override {
        std::string result;
        std::string content(data.begin(), data.end());
        
        // 查找PDF文本流
        size_t pos = 0;
        while ((pos = content.find("stream", pos)) != std::string::npos) {
            size_t start = pos + 6;
            // 跳过换行
            while (start < content.size() && (content[start] == '\r' || content[start] == '\n')) {
                start++;
            }
            
            size_t end = content.find("endstream", start);
            if (end != std::string::npos) {
                std::string stream_data = content.substr(start, end - start);
                
                // 尝试解压缩(简化处理,实际需要zlib)
                std::string decoded = decodeStream(stream_data);
                
                // 提取文本内容
                std::string text = extractTextFromStream(decoded);
                if (!text.empty()) {
                    result += text + " ";
                }
            }
            
            pos = end != std::string::npos ? end : pos + 1;
        }
        
        // 清理文本
        result = cleanText(result);
        
        return result;
    }
    
private:
    std::string decodeStream(const std::string& data) {
        // 简化实现,实际需要处理FlateDecode等压缩
        return data;
    }
    
    std::string extractTextFromStream(const std::string& stream) {
        std::string result;
        
        // 查找BT...ET文本块
        size_t pos = 0;
        while ((pos = stream.find("BT", pos)) != std::string::npos) {
            size_t end = stream.find("ET", pos);
            if (end != std::string::npos) {
                std::string text_block = stream.substr(pos + 2, end - pos - 2);
                
                // 提取Tj/TJ操作符的文本
                extractTjText(text_block, result);
            }
            pos = end != std::string::npos ? end + 2 : pos + 1;
        }
        
        return result;
    }
    
    void extractTjText(const std::string& block, std::string& result) {
        // 简化的文本提取
        size_t pos = 0;
        while (pos < block.size()) {
            // 查找字符串 ( ... )
            if (block[pos] == '(') {
                size_t end = pos + 1;
                int depth = 1;
                while (end < block.size() && depth > 0) {
                    if (block[end] == '(' && block[end-1] != '\\') depth++;
                    else if (block[end] == ')' && block[end-1] != '\\') depth--;
                    end++;
                }
                
                if (depth == 0) {
                    std::string text = block.substr(pos + 1, end - pos - 2);
                    result += decodeText(text) + " ";
                }
                pos = end;
            } else {
                pos++;
            }
        }
    }
    
    std::string decodeText(const std::string& text) {
        std::string result;
        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] == '\\' && i + 1 < text.size()) {
                switch (text[i + 1]) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '(': result += '('; break;
                    case ')': result += ')'; break;
                    case '\\': result += '\\'; break;
                    default: result += text[i + 1]; break;
                }
                i++;
            } else {
                result += text[i];
            }
        }
        return result;
    }
    
    void extractMetadata(const std::vector<uint8_t>& data, PDFDocument& doc) {
        std::string content(data.begin(), data.end());
        
        // 查找Title
        std::regex title_regex("/Title\\s*\\(([^)]*)\\)");
        std::smatch match;
        if (std::regex_search(content, match, title_regex)) {
            doc.title = match[1].str();
        }
        
        // 查找Author
        std::regex author_regex("/Author\\s*\\(([^)]*)\\)");
        if (std::regex_search(content, match, author_regex)) {
            doc.author = match[1].str();
        }
        
        // 统计页数
        size_t page_count = 0;
        size_t pos = 0;
        while ((pos = content.find("/Type /Page", pos)) != std::string::npos) {
            page_count++;
            pos++;
        }
        doc.page_count = page_count > 0 ? page_count : 1;
    }
    
    void extractStructure(PDFDocument& doc) {
        // 尝试识别摘要
        std::regex abstract_regex("(?:Abstract|ABSTRACT)[:\\s]*([\\s\\S]{100,2000}?)(?:Keywords|Introduction|1\\.|I\\.|$)", 
                                   std::regex::icase);
        std::smatch match;
        if (std::regex_search(doc.full_text, match, abstract_regex)) {
            doc.abstract = cleanText(match[1].str());
        }
        
        // 识别参考文献
        std::regex ref_regex("\\[\\d+\\]\\s*[A-Z][^\\[]{20,500}", std::regex::icase);
        std::sregex_iterator iter(doc.full_text.begin(), doc.full_text.end(), ref_regex);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
            doc.references.push_back(iter->str());
        }
    }
    
    std::string cleanText(const std::string& text) {
        std::string result;
        bool last_space = false;
        
        for (char c : text) {
            if (std::isspace(c)) {
                if (!last_space) {
                    result += ' ';
                    last_space = true;
                }
            } else if (std::isprint(c) || (unsigned char)c >= 128) {
                result += c;
                last_space = false;
            }
        }
        
        // 去除首尾空格
        size_t start = result.find_first_not_of(" \t\n\r");
        size_t end = result.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            return result.substr(start, end - start + 1);
        }
        return result;
    }
};

// ==================== 文本指纹生成器 ====================
class TextFingerprint {
public:
    // MinHash签名
    struct MinHashSignature {
        std::vector<uint64_t> signature;
        size_t num_hashes;
        
        MinHashSignature(size_t n = 128) : num_hashes(n), signature(n, UINT64_MAX) {}
        
        double estimateJaccard(const MinHashSignature& other) const {
            if (signature.size() != other.signature.size()) return 0.0;
            size_t matches = 0;
            for (size_t i = 0; i < signature.size(); i++) {
                if (signature[i] == other.signature[i]) matches++;
            }
            return static_cast<double>(matches) / signature.size();
        }
    };
    
    // SimHash签名
    struct SimHashSignature {
        uint64_t hash;
        
        SimHashSignature() : hash(0) {}
        
        int hammingDistance(const SimHashSignature& other) const {
            uint64_t diff = hash ^ other.hash;
            int count = 0;
            while (diff) {
                count += diff & 1;
                diff >>= 1;
            }
            return count;
        }
        
        double similarity(const SimHashSignature& other) const {
            return 1.0 - hammingDistance(other) / 64.0;
        }
    };
    
    // 生成N-gram
    static std::vector<std::string> generateNgrams(const std::string& text, int n = 3) {
        std::vector<std::string> ngrams;
        std::vector<std::string> words = tokenize(text);
        
        if (words.size() < static_cast<size_t>(n)) return ngrams;
        
        for (size_t i = 0; i <= words.size() - n; i++) {
            std::string ngram;
            for (int j = 0; j < n; j++) {
                if (j > 0) ngram += " ";
                ngram += words[i + j];
            }
            ngrams.push_back(ngram);
        }
        
        return ngrams;
    }
    
    // 生成字符级shingles
    static std::set<std::string> generateShingles(const std::string& text, int k = 5) {
        std::set<std::string> shingles;
        std::string cleaned = normalizeText(text);
        
        if (cleaned.size() < static_cast<size_t>(k)) {
            shingles.insert(cleaned);
            return shingles;
        }
        
        for (size_t i = 0; i <= cleaned.size() - k; i++) {
            shingles.insert(cleaned.substr(i, k));
        }
        
        return shingles;
    }
    
    // 计算MinHash签名
    static MinHashSignature computeMinHash(const std::set<std::string>& shingles, size_t num_hashes = 128) {
        MinHashSignature sig(num_hashes);
        
        for (const auto& shingle : shingles) {
            uint64_t base_hash = hashString(shingle);
            
            for (size_t i = 0; i < num_hashes; i++) {
                // 使用不同的哈希函数
                uint64_t h = (base_hash * (i + 1) + (i * 17)) % UINT64_MAX;
                if (h < sig.signature[i]) {
                    sig.signature[i] = h;
                }
            }
        }
        
        return sig;
    }
    
    // 计算SimHash签名
    static SimHashSignature computeSimHash(const std::string& text) {
        SimHashSignature sig;
        std::vector<std::string> words = tokenize(text);
        
        // 词频统计
        std::unordered_map<std::string, int> word_freq;
        for (const auto& word : words) {
            word_freq[word]++;
        }
        
        // 计算加权向量
        std::vector<int> v(64, 0);
        
        for (const auto& [word, freq] : word_freq) {
            uint64_t hash = hashString(word);
            for (int i = 0; i < 64; i++) {
                if (hash & (1ULL << i)) {
                    v[i] += freq;
                } else {
                    v[i] -= freq;
                }
            }
        }
        
        // 生成最终哈希
        for (int i = 0; i < 64; i++) {
            if (v[i] > 0) {
                sig.hash |= (1ULL << i);
            }
        }
        
        return sig;
    }
    
private:
    static std::vector<std::string> tokenize(const std::string& text) {
        std::vector<std::string> words;
        std::string word;
        
        for (char c : text) {
            if (std::isalnum(c)) {
                word += std::tolower(c);
            } else if (!word.empty()) {
                if (word.size() >= 2) {  // 过滤太短的词
                    words.push_back(word);
                }
                word.clear();
            }
        }
        
        if (!word.empty() && word.size() >= 2) {
            words.push_back(word);
        }
        
        return words;
    }
    
    static std::string normalizeText(const std::string& text) {
        std::string result;
        for (char c : text) {
            if (std::isalnum(c)) {
                result += std::tolower(c);
            }
        }
        return result;
    }
    
    static uint64_t hashString(const std::string& str) {
        uint64_t hash = 14695981039346656037ULL;  // FNV-1a
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

// ==================== 全文相似度检测器 ====================
class FullTextSimilarityDetector {
public:
    struct SimilarityResult {
        double overall_score;           // 总体相似度 0-1
        double text_similarity;         // 文本相似度
        double structure_similarity;    // 结构相似度
        double reference_overlap;       // 参考文献重合度
        
        std::vector<std::pair<std::string, std::string>> similar_passages;  // 相似段落对
        std::vector<std::string> common_references;                          // 共同参考文献
        
        std::string warning_level;      // LOW, MEDIUM, HIGH, CRITICAL
        std::string explanation;
        
        SimilarityResult() : overall_score(0), text_similarity(0),
                            structure_similarity(0), reference_overlap(0) {}
    };
    
    struct DocumentIndex {
        uint32_t doc_id;
        std::string title;
        TextFingerprint::MinHashSignature minhash;
        TextFingerprint::SimHashSignature simhash;
        std::vector<std::string> ngrams;
        std::set<std::string> shingles;
        std::string full_text;
        std::vector<std::string> references;
        std::vector<std::string> paragraphs;
        
        DocumentIndex() : doc_id(0) {}
    };
    
private:
    std::shared_ptr<PDFParser> parser_;
    std::vector<DocumentIndex> document_index_;
    mutable std::mutex index_mutex_;
    
    // 配置参数
    double critical_threshold_ = 0.80;
    double high_threshold_ = 0.60;
    double medium_threshold_ = 0.40;
    int shingle_size_ = 5;
    int ngram_size_ = 3;
    size_t minhash_num_ = 128;
    
public:
    FullTextSimilarityDetector() {
        parser_ = std::make_shared<SimplePDFParser>();
    }
    
    // 添加文档到索引
    uint32_t indexDocument(uint32_t doc_id, const std::vector<uint8_t>& pdf_data, const std::string& title = "") {
        PDFDocument pdf_doc;
        if (!parser_->parseMemory(pdf_data, pdf_doc)) {
            // 如果PDF解析失败,尝试作为纯文本处理
            pdf_doc.full_text = std::string(pdf_data.begin(), pdf_data.end());
        }
        
        return indexDocument(doc_id, pdf_doc.full_text, title, pdf_doc.references);
    }
    
    uint32_t indexDocument(uint32_t doc_id, const std::string& text, const std::string& title = "",
                           const std::vector<std::string>& references = {}) {
        std::lock_guard<std::mutex> lock(index_mutex_);
        
        DocumentIndex idx;
        idx.doc_id = doc_id;
        idx.title = title;
        idx.full_text = text;
        idx.references = references;
        
        // 生成指纹
        idx.shingles = TextFingerprint::generateShingles(text, shingle_size_);
        idx.minhash = TextFingerprint::computeMinHash(idx.shingles, minhash_num_);
        idx.simhash = TextFingerprint::computeSimHash(text);
        idx.ngrams = TextFingerprint::generateNgrams(text, ngram_size_);
        
        // 分割段落
        idx.paragraphs = splitParagraphs(text);
        
        document_index_.push_back(idx);
        
        return doc_id;
    }
    
    // 检测与所有已索引文档的相似度
    std::vector<SimilarityResult> detectSimilarity(uint32_t doc_id, const std::vector<uint8_t>& pdf_data) {
        PDFDocument pdf_doc;
        if (!parser_->parseMemory(pdf_data, pdf_doc)) {
            pdf_doc.full_text = std::string(pdf_data.begin(), pdf_data.end());
        }
        
        return detectSimilarity(doc_id, pdf_doc.full_text, pdf_doc.references);
    }
    
    std::vector<SimilarityResult> detectSimilarity(uint32_t doc_id, const std::string& text,
                                                    const std::vector<std::string>& references = {}) {
        std::vector<SimilarityResult> results;
        
        // 计算查询文档的指纹
        auto shingles = TextFingerprint::generateShingles(text, shingle_size_);
        auto minhash = TextFingerprint::computeMinHash(shingles, minhash_num_);
        auto simhash = TextFingerprint::computeSimHash(text);
        auto paragraphs = splitParagraphs(text);
        
        std::lock_guard<std::mutex> lock(index_mutex_);
        
        for (const auto& idx : document_index_) {
            if (idx.doc_id == doc_id) continue;  // 跳过自身
            
            SimilarityResult result;
            
            // 1. MinHash Jaccard估计
            double minhash_sim = minhash.estimateJaccard(idx.minhash);
            
            // 2. SimHash相似度
            double simhash_sim = simhash.similarity(idx.simhash);
            
            // 3. 详细文本比较(如果初步相似度较高)
            if (minhash_sim > 0.2 || simhash_sim > 0.7) {
                result.text_similarity = computeDetailedTextSimilarity(text, idx.full_text, shingles, idx.shingles);
            } else {
                result.text_similarity = (minhash_sim + simhash_sim) / 2;
            }
            
            // 4. 参考文献重合度
            if (!references.empty() && !idx.references.empty()) {
                result.reference_overlap = computeReferenceOverlap(references, idx.references);
            }
            
            // 5. 查找相似段落
            if (result.text_similarity > medium_threshold_) {
                result.similar_passages = findSimilarPassages(paragraphs, idx.paragraphs);
            }
            
            // 6. 计算总体得分
            result.overall_score = computeOverallScore(result);
            
            // 7. 确定警告级别
            determineWarningLevel(result);
            
            if (result.overall_score > 0.1) {  // 只返回有意义的结果
                results.push_back(result);
            }
        }
        
        // 按相似度排序
        std::sort(results.begin(), results.end(),
                  [](const SimilarityResult& a, const SimilarityResult& b) {
                      return a.overall_score > b.overall_score;
                  });
        
        return results;
    }
    
    // 快速筛选候选文档(LSH)
    std::vector<uint32_t> findCandidates(const std::string& text, double threshold = 0.3) {
        std::vector<uint32_t> candidates;
        
        auto simhash = TextFingerprint::computeSimHash(text);
        
        std::lock_guard<std::mutex> lock(index_mutex_);
        
        for (const auto& idx : document_index_) {
            // 使用SimHash快速筛选
            double sim = simhash.similarity(idx.simhash);
            if (sim >= threshold) {
                candidates.push_back(idx.doc_id);
            }
        }
        
        return candidates;
    }
    
    // 两两文档比较
    SimilarityResult compareDocuments(const std::string& text1, const std::string& text2) {
        SimilarityResult result;
        
        auto shingles1 = TextFingerprint::generateShingles(text1, shingle_size_);
        auto shingles2 = TextFingerprint::generateShingles(text2, shingle_size_);
        
        result.text_similarity = computeDetailedTextSimilarity(text1, text2, shingles1, shingles2);
        result.overall_score = result.text_similarity;
        
        auto paragraphs1 = splitParagraphs(text1);
        auto paragraphs2 = splitParagraphs(text2);
        result.similar_passages = findSimilarPassages(paragraphs1, paragraphs2);
        
        determineWarningLevel(result);
        
        return result;
    }
    
    // 获取索引大小
    size_t getIndexSize() const {
        std::lock_guard<std::mutex> lock(index_mutex_);
        return document_index_.size();
    }
    
    // 清除索引
    void clearIndex() {
        std::lock_guard<std::mutex> lock(index_mutex_);
        document_index_.clear();
    }
    
    // 设置阈值
    void setThresholds(double critical, double high, double medium) {
        critical_threshold_ = critical;
        high_threshold_ = high;
        medium_threshold_ = medium;
    }
    
private:
    std::vector<std::string> splitParagraphs(const std::string& text, size_t min_length = 100) {
        std::vector<std::string> paragraphs;
        std::string current;
        
        for (char c : text) {
            if (c == '\n' && current.size() >= min_length) {
                paragraphs.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        
        if (current.size() >= min_length) {
            paragraphs.push_back(current);
        }
        
        // 如果段落太少,按句子分割
        if (paragraphs.size() < 5) {
            paragraphs.clear();
            std::stringstream ss(text);
            std::string sentence;
            std::string buffer;
            
            while (std::getline(ss, sentence, '.')) {
                buffer += sentence + ".";
                if (buffer.size() >= min_length) {
                    paragraphs.push_back(buffer);
                    buffer.clear();
                }
            }
            
            if (!buffer.empty()) {
                paragraphs.push_back(buffer);
            }
        }
        
        return paragraphs;
    }
    
    double computeDetailedTextSimilarity(const std::string& text1, const std::string& text2,
                                          const std::set<std::string>& shingles1,
                                          const std::set<std::string>& shingles2) {
        // Jaccard相似度
        std::set<std::string> intersection;
        std::set_intersection(shingles1.begin(), shingles1.end(),
                              shingles2.begin(), shingles2.end(),
                              std::inserter(intersection, intersection.begin()));
        
        std::set<std::string> union_set;
        std::set_union(shingles1.begin(), shingles1.end(),
                       shingles2.begin(), shingles2.end(),
                       std::inserter(union_set, union_set.begin()));
        
        if (union_set.empty()) return 0.0;
        
        double jaccard = static_cast<double>(intersection.size()) / union_set.size();
        
        // LCS相似度(采样计算以提高效率)
        double lcs_sim = 0.0;
        if (text1.size() < 10000 && text2.size() < 10000) {
            lcs_sim = computeLCSSimilarity(text1, text2);
        }
        
        // 综合
        return 0.6 * jaccard + 0.4 * lcs_sim;
    }
    
    double computeLCSSimilarity(const std::string& s1, const std::string& s2) {
        // 使用空间优化的LCS
        std::string short_str = s1.size() < s2.size() ? s1 : s2;
        std::string long_str = s1.size() < s2.size() ? s2 : s1;
        
        size_t m = short_str.size();
        size_t n = long_str.size();
        
        if (m == 0 || n == 0) return 0.0;
        
        // 采样以限制计算量
        if (m > 1000) {
            short_str = short_str.substr(0, 500) + short_str.substr(m - 500);
            m = 1000;
        }
        if (n > 1000) {
            long_str = long_str.substr(0, 500) + long_str.substr(n - 500);
            n = 1000;
        }
        
        std::vector<size_t> prev(n + 1, 0);
        std::vector<size_t> curr(n + 1, 0);
        
        for (size_t i = 1; i <= m; i++) {
            for (size_t j = 1; j <= n; j++) {
                if (std::tolower(short_str[i-1]) == std::tolower(long_str[j-1])) {
                    curr[j] = prev[j-1] + 1;
                } else {
                    curr[j] = std::max(prev[j], curr[j-1]);
                }
            }
            std::swap(prev, curr);
            std::fill(curr.begin(), curr.end(), 0);
        }
        
        size_t lcs_length = prev[n];
        return 2.0 * lcs_length / (m + n);
    }
    
    double computeReferenceOverlap(const std::vector<std::string>& refs1,
                                    const std::vector<std::string>& refs2) {
        if (refs1.empty() || refs2.empty()) return 0.0;
        
        // 提取参考文献标识符(作者年份)
        auto extractKeys = [](const std::vector<std::string>& refs) {
            std::set<std::string> keys;
            std::regex author_year_regex("([A-Z][a-z]+).*?(19|20)\\d{2}");
            for (const auto& ref : refs) {
                std::smatch match;
                if (std::regex_search(ref, match, author_year_regex)) {
                    keys.insert(match[1].str() + match[2].str());
                }
            }
            return keys;
        };
        
        auto keys1 = extractKeys(refs1);
        auto keys2 = extractKeys(refs2);
        
        if (keys1.empty() || keys2.empty()) return 0.0;
        
        std::set<std::string> intersection;
        std::set_intersection(keys1.begin(), keys1.end(),
                              keys2.begin(), keys2.end(),
                              std::inserter(intersection, intersection.begin()));
        
        return static_cast<double>(intersection.size()) / std::min(keys1.size(), keys2.size());
    }
    
    std::vector<std::pair<std::string, std::string>> findSimilarPassages(
            const std::vector<std::string>& paras1,
            const std::vector<std::string>& paras2,
            double threshold = 0.5, size_t max_pairs = 5) {
        
        std::vector<std::pair<std::string, std::string>> similar_pairs;
        
        for (const auto& p1 : paras1) {
            if (p1.size() < 50) continue;
            
            auto shingles1 = TextFingerprint::generateShingles(p1, 4);
            
            for (const auto& p2 : paras2) {
                if (p2.size() < 50) continue;
                
                auto shingles2 = TextFingerprint::generateShingles(p2, 4);
                
                // 计算Jaccard
                std::set<std::string> intersection;
                std::set_intersection(shingles1.begin(), shingles1.end(),
                                      shingles2.begin(), shingles2.end(),
                                      std::inserter(intersection, intersection.begin()));
                
                size_t union_size = shingles1.size() + shingles2.size() - intersection.size();
                double sim = union_size > 0 ? static_cast<double>(intersection.size()) / union_size : 0;
                
                if (sim >= threshold) {
                    // 截取摘要
                    std::string excerpt1 = p1.size() > 200 ? p1.substr(0, 200) + "..." : p1;
                    std::string excerpt2 = p2.size() > 200 ? p2.substr(0, 200) + "..." : p2;
                    similar_pairs.push_back({excerpt1, excerpt2});
                    
                    if (similar_pairs.size() >= max_pairs) {
                        return similar_pairs;
                    }
                }
            }
        }
        
        return similar_pairs;
    }
    
    double computeOverallScore(const SimilarityResult& result) {
        double score = 0.7 * result.text_similarity +
                       0.2 * result.structure_similarity +
                       0.1 * result.reference_overlap;
        
        // 如果发现多个相似段落,增加权重
        if (result.similar_passages.size() >= 3) {
            score = std::min(1.0, score * 1.2);
        }
        
        return score;
    }
    
    void determineWarningLevel(SimilarityResult& result) {
        if (result.overall_score >= critical_threshold_) {
            result.warning_level = "CRITICAL";
            result.explanation = "Extremely high similarity detected. Manual review required.";
        } else if (result.overall_score >= high_threshold_) {
            result.warning_level = "HIGH";
            result.explanation = "Significant overlap found. Recommend detailed comparison.";
        } else if (result.overall_score >= medium_threshold_) {
            result.warning_level = "MEDIUM";
            result.explanation = "Moderate similarity. May contain common topics or citations.";
        } else {
            result.warning_level = "LOW";
            result.explanation = "Low similarity. Likely independent work.";
        }
    }
};

// ==================== 相似度报告生成器 ====================
class SimilarityReportGenerator {
public:
    static std::string generateTextReport(const std::vector<FullTextSimilarityDetector::SimilarityResult>& results,
                                          const std::string& doc_title) {
        std::stringstream ss;
        
        ss << "═══════════════════════════════════════════════════════════\n";
        ss << "           SIMILARITY DETECTION REPORT\n";
        ss << "═══════════════════════════════════════════════════════════\n\n";
        ss << "Document: " << doc_title << "\n";
        ss << "Analysis Date: " << getCurrentTimeString() << "\n";
        ss << "Documents Compared: " << results.size() << "\n\n";
        
        // 统计摘要
        int critical = 0, high = 0, medium = 0, low = 0;
        for (const auto& r : results) {
            if (r.warning_level == "CRITICAL") critical++;
            else if (r.warning_level == "HIGH") high++;
            else if (r.warning_level == "MEDIUM") medium++;
            else low++;
        }
        
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "                    SUMMARY\n";
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "  🔴 CRITICAL: " << critical << "\n";
        ss << "  🟠 HIGH:     " << high << "\n";
        ss << "  🟡 MEDIUM:   " << medium << "\n";
        ss << "  🟢 LOW:      " << low << "\n\n";
        
        if (critical > 0 || high > 0) {
            ss << "⚠️  WARNING: Potential plagiarism detected!\n\n";
        }
        
        // 详细结果
        ss << "───────────────────────────────────────────────────────────\n";
        ss << "                 DETAILED RESULTS\n";
        ss << "───────────────────────────────────────────────────────────\n\n";
        
        int rank = 1;
        for (const auto& r : results) {
            ss << "Match #" << rank++ << "\n";
            ss << "  Overall Similarity: " << std::fixed << std::setprecision(1) 
               << (r.overall_score * 100) << "%\n";
            ss << "  Warning Level: " << r.warning_level << "\n";
            ss << "  Text Similarity: " << std::fixed << std::setprecision(1)
               << (r.text_similarity * 100) << "%\n";
            ss << "  Reference Overlap: " << std::fixed << std::setprecision(1)
               << (r.reference_overlap * 100) << "%\n";
            ss << "  Analysis: " << r.explanation << "\n";
            
            if (!r.similar_passages.empty()) {
                ss << "\n  Similar Passages Found:\n";
                for (size_t i = 0; i < r.similar_passages.size() && i < 3; i++) {
                    ss << "  ─────\n";
                    ss << "  Doc1: \"" << r.similar_passages[i].first.substr(0, 100) << "...\"\n";
                    ss << "  Doc2: \"" << r.similar_passages[i].second.substr(0, 100) << "...\"\n";
                }
            }
            
            ss << "\n";
        }
        
        ss << "═══════════════════════════════════════════════════════════\n";
        ss << "                    END OF REPORT\n";
        ss << "═══════════════════════════════════════════════════════════\n";
        
        return ss.str();
    }
    
    static std::string generateHTMLReport(const std::vector<FullTextSimilarityDetector::SimilarityResult>& results,
                                          const std::string& doc_title) {
        std::stringstream ss;
        
        ss << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Similarity Report</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; }
        .summary { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; margin-bottom: 20px; }
        .stat-card { background: white; padding: 20px; border-radius: 10px; text-align: center; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .stat-value { font-size: 36px; font-weight: bold; }
        .critical { color: #dc3545; }
        .high { color: #fd7e14; }
        .medium { color: #ffc107; }
        .low { color: #28a745; }
        .result-card { background: white; padding: 20px; border-radius: 10px; margin-bottom: 15px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .result-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; }
        .score-bar { height: 10px; background: #e9ecef; border-radius: 5px; overflow: hidden; }
        .score-fill { height: 100%; transition: width 0.3s; }
        .badge { display: inline-block; padding: 5px 15px; border-radius: 20px; color: white; font-weight: bold; }
        .badge-critical { background: #dc3545; }
        .badge-high { background: #fd7e14; }
        .badge-medium { background: #ffc107; color: #333; }
        .badge-low { background: #28a745; }
        .passage { background: #f8f9fa; padding: 15px; border-left: 4px solid #667eea; margin: 10px 0; font-style: italic; }
        table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid #dee2e6; }
        th { background: #f8f9fa; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📊 Similarity Detection Report</h1>
            <p>Document: )" << escapeHtml(doc_title) << R"(</p>
            <p>Generated: )" << getCurrentTimeString() << R"(</p>
        </div>
)";
        
        // 统计
        int critical = 0, high = 0, medium = 0, low = 0;
        for (const auto& r : results) {
            if (r.warning_level == "CRITICAL") critical++;
            else if (r.warning_level == "HIGH") high++;
            else if (r.warning_level == "MEDIUM") medium++;
            else low++;
        }
        
        ss << R"(
        <div class="summary">
            <div class="stat-card">
                <div class="stat-value critical">)" << critical << R"(</div>
                <div>Critical</div>
            </div>
            <div class="stat-card">
                <div class="stat-value high">)" << high << R"(</div>
                <div>High</div>
            </div>
            <div class="stat-card">
                <div class="stat-value medium">)" << medium << R"(</div>
                <div>Medium</div>
            </div>
            <div class="stat-card">
                <div class="stat-value low">)" << low << R"(</div>
                <div>Low</div>
            </div>
        </div>
)";
        
        // 详细结果
        int rank = 1;
        for (const auto& r : results) {
            std::string badge_class = "badge-low";
            std::string color = "#28a745";
            if (r.warning_level == "CRITICAL") { badge_class = "badge-critical"; color = "#dc3545"; }
            else if (r.warning_level == "HIGH") { badge_class = "badge-high"; color = "#fd7e14"; }
            else if (r.warning_level == "MEDIUM") { badge_class = "badge-medium"; color = "#ffc107"; }
            
            ss << R"(
        <div class="result-card">
            <div class="result-header">
                <h3>Match #)" << rank++ << R"(</h3>
                <span class="badge )" << badge_class << R"(">)" << r.warning_level << R"(</span>
            </div>
            <div class="score-bar">
                <div class="score-fill" style="width: )" << (r.overall_score * 100) << R"(%; background: )" << color << R"(;"></div>
            </div>
            <table>
                <tr><th>Metric</th><th>Value</th></tr>
                <tr><td>Overall Similarity</td><td>)" << std::fixed << std::setprecision(1) << (r.overall_score * 100) << R"(%</td></tr>
                <tr><td>Text Similarity</td><td>)" << (r.text_similarity * 100) << R"(%</td></tr>
                <tr><td>Reference Overlap</td><td>)" << (r.reference_overlap * 100) << R"(%</td></tr>
            </table>
            <p><strong>Analysis:</strong> )" << escapeHtml(r.explanation) << R"(</p>
)";
            
            if (!r.similar_passages.empty()) {
                ss << R"(
            <h4>Similar Passages:</h4>
)";
                for (const auto& [p1, p2] : r.similar_passages) {
                    ss << R"(
            <div class="passage">")" << escapeHtml(p1.substr(0, 150)) << R"(..."</div>
            <div class="passage">")" << escapeHtml(p2.substr(0, 150)) << R"(..."</div>
)";
                }
            }
            
            ss << R"(
        </div>
)";
        }
        
        ss << R"(
    </div>
</body>
</html>
)";
        
        return ss.str();
    }
    
private:
    static std::string getCurrentTimeString() {
        time_t now = time(nullptr);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string(buffer);
    }
    
    static std::string escapeHtml(const std::string& text) {
        std::string result;
        for (char c : text) {
            switch (c) {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&#39;"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

#endif // PDF_ANALYZER_H
