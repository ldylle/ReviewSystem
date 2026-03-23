#ifndef SIMILARITY_DETECTOR_H
#define SIMILARITY_DETECTOR_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <cmath>
#include <algorithm>
#include <sstream>

// ==================== 相似度检测结果 ====================
struct SimilarityResult {
    uint32_t paper_id;
    std::string paper_title;
    double overall_score;       // 总体相似度 0-1
    double title_similarity;    // 标题相似度
    double abstract_similarity; // 摘要相似度
    double keyword_similarity;  // 关键词相似度
    std::vector<std::string> common_keywords;
    std::string warning_level;  // "LOW", "MEDIUM", "HIGH", "CRITICAL"
    
    SimilarityResult() : paper_id(0), overall_score(0), 
                         title_similarity(0), abstract_similarity(0), 
                         keyword_similarity(0) {}
};

// ==================== 文本预处理器 ====================
class TextPreprocessor {
public:
    // 分词
    static std::vector<std::string> tokenize(const std::string& text);
    
    // 转小写
    static std::string to_lower(const std::string& text);
    
    // 去除停用词
    static std::vector<std::string> remove_stopwords(const std::vector<std::string>& tokens);
    
    // 词干提取(简化版)
    static std::string stem(const std::string& word);
    
    // N-gram生成
    static std::vector<std::string> generate_ngrams(const std::vector<std::string>& tokens, size_t n);
    
    // 获取停用词列表
    static const std::set<std::string>& get_stopwords();
};

// ==================== 相似度算法 ====================
class SimilarityAlgorithm {
public:
    // Jaccard相似度
    static double jaccard_similarity(const std::set<std::string>& set1, 
                                    const std::set<std::string>& set2);
    
    // 余弦相似度
    static double cosine_similarity(const std::map<std::string, double>& vec1,
                                   const std::map<std::string, double>& vec2);
    
    // 编辑距离(Levenshtein)
    static int edit_distance(const std::string& s1, const std::string& s2);
    
    // 基于编辑距离的相似度
    static double edit_distance_similarity(const std::string& s1, const std::string& s2);
    
    // 最长公共子序列
    static int lcs_length(const std::string& s1, const std::string& s2);
    
    // 基于LCS的相似度
    static double lcs_similarity(const std::string& s1, const std::string& s2);
    
    // SimHash (局部敏感哈希)
    static uint64_t simhash(const std::vector<std::string>& tokens);
    
    // SimHash汉明距离
    static int simhash_distance(uint64_t hash1, uint64_t hash2);
};

// ==================== TF-IDF 向量化器 ====================
class TFIDFVectorizer {
private:
    std::map<std::string, double> idf_scores_;  // 逆文档频率
    std::map<std::string, int> document_frequency_;  // 文档频率
    int total_documents_;
    
public:
    TFIDFVectorizer() : total_documents_(0) {}
    
    // 训练(添加文档)
    void add_document(const std::vector<std::string>& tokens);
    
    // 计算IDF
    void compute_idf();
    
    // 转换为TF-IDF向量
    std::map<std::string, double> transform(const std::vector<std::string>& tokens) const;
    
    // 获取词汇表大小
    size_t vocabulary_size() const { return idf_scores_.size(); }
};

// ==================== 论文相似度检测器 ====================
class PaperSimilarityDetector {
private:
    // 已有论文的特征缓存
    struct PaperFeatures {
        uint32_t paper_id;
        std::string title;
        std::set<std::string> title_tokens;
        std::set<std::string> abstract_tokens;
        std::set<std::string> keywords;
        std::map<std::string, double> tfidf_vector;
        uint64_t simhash;
    };
    
    std::map<uint32_t, PaperFeatures> paper_cache_;
    TFIDFVectorizer vectorizer_;
    
    // 阈值配置
    double title_threshold_;
    double abstract_threshold_;
    double keyword_threshold_;
    double overall_threshold_;
    
public:
    PaperSimilarityDetector();
    
    // 添加论文到索引
    void index_paper(uint32_t paper_id, const std::string& title,
                    const std::string& abstract,
                    const std::vector<std::string>& keywords);
    
    // 移除论文
    void remove_paper(uint32_t paper_id);
    
    // 检测相似论文
    std::vector<SimilarityResult> detect_similar(
        const std::string& title,
        const std::string& abstract,
        const std::vector<std::string>& keywords,
        double min_similarity = 0.3) const;
    
    // 快速检测(使用SimHash)
    std::vector<uint32_t> quick_detect(const std::string& abstract, int max_distance = 10) const;
    
    // 计算两篇论文的详细相似度
    SimilarityResult compute_similarity(uint32_t paper_id,
                                       const std::string& title,
                                       const std::string& abstract,
                                       const std::vector<std::string>& keywords) const;
    
    // 设置阈值
    void set_thresholds(double title, double abstract, double keyword, double overall);
    
    // 获取警告级别
    static std::string get_warning_level(double similarity);
    
    // 统计信息
    size_t get_indexed_count() const { return paper_cache_.size(); }
    
    // 重建索引
    void rebuild_index();
};

// ==================== 查重报告生成器 ====================
class PlagiarismReportGenerator {
public:
    struct ReportSection {
        std::string section_name;
        std::string content;
        std::vector<std::pair<std::string, double>> matches;  // (来源, 相似度)
    };
    
    struct FullReport {
        uint32_t paper_id;
        std::string paper_title;
        time_t check_time;
        double overall_similarity;
        std::vector<SimilarityResult> similar_papers;
        std::vector<ReportSection> sections;
        std::string recommendation;  // "PASS", "REVIEW", "REJECT"
    };
    
    // 生成完整报告
    static FullReport generate_report(
        uint32_t paper_id,
        const std::string& title,
        const std::string& abstract,
        const std::vector<std::string>& keywords,
        const std::vector<SimilarityResult>& results);
    
    // 格式化为文本
    static std::string format_text_report(const FullReport& report);
    
    // 格式化为JSON
    static std::string format_json_report(const FullReport& report);
    
    // 格式化为彩色终端输出
    static std::string format_colored_report(const FullReport& report);
};

#endif // SIMILARITY_DETECTOR_H
