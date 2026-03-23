#include "similarity_detector.h"
#include <cstring>
#include <numeric>
#include <functional>
#include <iomanip>

// ==================== TextPreprocessor 实现 ====================
std::vector<std::string> TextPreprocessor::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current_word;
    
    for (char c : text) {
        if (std::isalnum(c)) {
            current_word += std::tolower(c);
        } else if (!current_word.empty()) {
            tokens.push_back(current_word);
            current_word.clear();
        }
    }
    if (!current_word.empty()) {
        tokens.push_back(current_word);
    }
    
    return tokens;
}

std::string TextPreprocessor::to_lower(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> TextPreprocessor::remove_stopwords(const std::vector<std::string>& tokens) {
    const auto& stopwords = get_stopwords();
    std::vector<std::string> result;
    
    for (const auto& token : tokens) {
        if (stopwords.find(token) == stopwords.end()) {
            result.push_back(token);
        }
    }
    
    return result;
}

std::string TextPreprocessor::stem(const std::string& word) {
    // 简化的词干提取：去除常见后缀
    std::string result = word;
    
    if (result.length() > 4) {
        if (result.substr(result.length() - 3) == "ing") {
            result = result.substr(0, result.length() - 3);
        } else if (result.substr(result.length() - 2) == "ed") {
            result = result.substr(0, result.length() - 2);
        } else if (result.substr(result.length() - 2) == "ly") {
            result = result.substr(0, result.length() - 2);
        } else if (result.substr(result.length() - 1) == "s" && result.length() > 3) {
            result = result.substr(0, result.length() - 1);
        }
    }
    
    return result;
}

std::vector<std::string> TextPreprocessor::generate_ngrams(const std::vector<std::string>& tokens, size_t n) {
    std::vector<std::string> ngrams;
    
    if (tokens.size() < n) return ngrams;
    
    for (size_t i = 0; i <= tokens.size() - n; ++i) {
        std::string ngram;
        for (size_t j = 0; j < n; ++j) {
            if (j > 0) ngram += " ";
            ngram += tokens[i + j];
        }
        ngrams.push_back(ngram);
    }
    
    return ngrams;
}

const std::set<std::string>& TextPreprocessor::get_stopwords() {
    static std::set<std::string> stopwords = {
        "a", "an", "the", "and", "or", "but", "in", "on", "at", "to", "for",
        "of", "with", "by", "from", "as", "is", "was", "are", "were", "been",
        "be", "have", "has", "had", "do", "does", "did", "will", "would", "could",
        "should", "may", "might", "must", "shall", "can", "need", "this", "that",
        "these", "those", "i", "you", "he", "she", "it", "we", "they", "what",
        "which", "who", "whom", "whose", "where", "when", "why", "how", "all",
        "each", "every", "both", "few", "more", "most", "other", "some", "such",
        "no", "nor", "not", "only", "own", "same", "so", "than", "too", "very",
        "just", "also", "now", "here", "there", "then", "once", "paper", "method",
        "approach", "propose", "proposed", "present", "presented", "show", "shows"
    };
    return stopwords;
}

// ==================== SimilarityAlgorithm 实现 ====================
double SimilarityAlgorithm::jaccard_similarity(const std::set<std::string>& set1,
                                               const std::set<std::string>& set2) {
    if (set1.empty() && set2.empty()) return 1.0;
    if (set1.empty() || set2.empty()) return 0.0;
    
    std::set<std::string> intersection, union_set;
    
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                         std::inserter(intersection, intersection.begin()));
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(),
                  std::inserter(union_set, union_set.begin()));
    
    return static_cast<double>(intersection.size()) / union_set.size();
}

double SimilarityAlgorithm::cosine_similarity(const std::map<std::string, double>& vec1,
                                              const std::map<std::string, double>& vec2) {
    if (vec1.empty() || vec2.empty()) return 0.0;
    
    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;
    
    for (const auto& [key, val] : vec1) {
        norm1 += val * val;
        auto it = vec2.find(key);
        if (it != vec2.end()) {
            dot_product += val * it->second;
        }
    }
    
    for (const auto& [key, val] : vec2) {
        norm2 += val * val;
    }
    
    if (norm1 == 0 || norm2 == 0) return 0.0;
    
    return dot_product / (std::sqrt(norm1) * std::sqrt(norm2));
}

int SimilarityAlgorithm::edit_distance(const std::string& s1, const std::string& s2) {
    size_t m = s1.length();
    size_t n = s2.length();
    
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    
    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;
    
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    
    return dp[m][n];
}

double SimilarityAlgorithm::edit_distance_similarity(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 1.0;
    
    int distance = edit_distance(s1, s2);
    int max_len = std::max(s1.length(), s2.length());
    
    return 1.0 - static_cast<double>(distance) / max_len;
}

int SimilarityAlgorithm::lcs_length(const std::string& s1, const std::string& s2) {
    size_t m = s1.length();
    size_t n = s2.length();
    
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1] + 1;
            } else {
                dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
            }
        }
    }
    
    return dp[m][n];
}

double SimilarityAlgorithm::lcs_similarity(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 1.0;
    if (s1.empty() || s2.empty()) return 0.0;
    
    int lcs = lcs_length(s1, s2);
    int max_len = std::max(s1.length(), s2.length());
    
    return static_cast<double>(lcs) / max_len;
}

uint64_t SimilarityAlgorithm::simhash(const std::vector<std::string>& tokens) {
    std::vector<int> v(64, 0);
    
    for (const auto& token : tokens) {
        // 简单的哈希函数
        uint64_t hash = std::hash<std::string>{}(token);
        
        for (int i = 0; i < 64; ++i) {
            if ((hash >> i) & 1) {
                v[i]++;
            } else {
                v[i]--;
            }
        }
    }
    
    uint64_t fingerprint = 0;
    for (int i = 0; i < 64; ++i) {
        if (v[i] > 0) {
            fingerprint |= (1ULL << i);
        }
    }
    
    return fingerprint;
}

int SimilarityAlgorithm::simhash_distance(uint64_t hash1, uint64_t hash2) {
    uint64_t diff = hash1 ^ hash2;
    int distance = 0;
    
    while (diff) {
        distance++;
        diff &= (diff - 1);
    }
    
    return distance;
}

// ==================== TFIDFVectorizer 实现 ====================
void TFIDFVectorizer::add_document(const std::vector<std::string>& tokens) {
    std::set<std::string> unique_tokens(tokens.begin(), tokens.end());
    
    for (const auto& token : unique_tokens) {
        document_frequency_[token]++;
    }
    
    total_documents_++;
}

void TFIDFVectorizer::compute_idf() {
    for (const auto& [term, df] : document_frequency_) {
        idf_scores_[term] = std::log(static_cast<double>(total_documents_ + 1) / (df + 1)) + 1.0;
    }
}

std::map<std::string, double> TFIDFVectorizer::transform(const std::vector<std::string>& tokens) const {
    std::map<std::string, double> result;
    
    // 计算词频
    std::map<std::string, int> tf;
    for (const auto& token : tokens) {
        tf[token]++;
    }
    
    // 计算TF-IDF
    for (const auto& [term, count] : tf) {
        double tfidf = static_cast<double>(count) / tokens.size();
        
        auto it = idf_scores_.find(term);
        if (it != idf_scores_.end()) {
            tfidf *= it->second;
        } else {
            // 未见过的词，使用最大IDF
            tfidf *= std::log(static_cast<double>(total_documents_ + 1)) + 1.0;
        }
        
        result[term] = tfidf;
    }
    
    return result;
}

// ==================== PaperSimilarityDetector 实现 ====================
PaperSimilarityDetector::PaperSimilarityDetector()
    : title_threshold_(0.7), abstract_threshold_(0.5),
      keyword_threshold_(0.6), overall_threshold_(0.5) {
}

void PaperSimilarityDetector::index_paper(uint32_t paper_id, const std::string& title,
                                          const std::string& abstract,
                                          const std::vector<std::string>& keywords) {
    PaperFeatures features;
    features.paper_id = paper_id;
    features.title = title;
    
    // 处理标题
    auto title_tokens = TextPreprocessor::tokenize(title);
    title_tokens = TextPreprocessor::remove_stopwords(title_tokens);
    features.title_tokens = std::set<std::string>(title_tokens.begin(), title_tokens.end());
    
    // 处理摘要
    auto abstract_tokens = TextPreprocessor::tokenize(abstract);
    abstract_tokens = TextPreprocessor::remove_stopwords(abstract_tokens);
    features.abstract_tokens = std::set<std::string>(abstract_tokens.begin(), abstract_tokens.end());
    
    // 处理关键词
    for (const auto& kw : keywords) {
        features.keywords.insert(TextPreprocessor::to_lower(kw));
    }
    
    // 计算SimHash
    features.simhash = SimilarityAlgorithm::simhash(abstract_tokens);
    
    // 添加到TF-IDF向量化器
    vectorizer_.add_document(abstract_tokens);
    
    paper_cache_[paper_id] = features;
}

void PaperSimilarityDetector::remove_paper(uint32_t paper_id) {
    paper_cache_.erase(paper_id);
}

std::vector<SimilarityResult> PaperSimilarityDetector::detect_similar(
    const std::string& title,
    const std::string& abstract,
    const std::vector<std::string>& keywords,
    double min_similarity) const {
    
    std::vector<SimilarityResult> results;
    
    // 处理输入
    auto title_tokens = TextPreprocessor::tokenize(title);
    title_tokens = TextPreprocessor::remove_stopwords(title_tokens);
    std::set<std::string> title_set(title_tokens.begin(), title_tokens.end());
    
    auto abstract_tokens = TextPreprocessor::tokenize(abstract);
    abstract_tokens = TextPreprocessor::remove_stopwords(abstract_tokens);
    std::set<std::string> abstract_set(abstract_tokens.begin(), abstract_tokens.end());
    
    std::set<std::string> keyword_set;
    for (const auto& kw : keywords) {
        keyword_set.insert(TextPreprocessor::to_lower(kw));
    }
    
    // 计算SimHash用于快速过滤
    uint64_t input_simhash = SimilarityAlgorithm::simhash(abstract_tokens);
    
    for (const auto& [pid, features] : paper_cache_) {
        // 快速过滤：SimHash距离过大则跳过
        int simhash_dist = SimilarityAlgorithm::simhash_distance(input_simhash, features.simhash);
        if (simhash_dist > 20) continue;  // 距离阈值
        
        SimilarityResult result;
        result.paper_id = pid;
        result.paper_title = features.title;
        
        // 计算各项相似度
        result.title_similarity = SimilarityAlgorithm::jaccard_similarity(title_set, features.title_tokens);
        result.abstract_similarity = SimilarityAlgorithm::jaccard_similarity(abstract_set, features.abstract_tokens);
        result.keyword_similarity = SimilarityAlgorithm::jaccard_similarity(keyword_set, features.keywords);
        
        // 找出共同关键词
        std::set_intersection(keyword_set.begin(), keyword_set.end(),
                             features.keywords.begin(), features.keywords.end(),
                             std::back_inserter(result.common_keywords));
        
        // 计算加权总分
        result.overall_score = 0.3 * result.title_similarity +
                              0.5 * result.abstract_similarity +
                              0.2 * result.keyword_similarity;
        
        result.warning_level = get_warning_level(result.overall_score);
        
        if (result.overall_score >= min_similarity) {
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

std::vector<uint32_t> PaperSimilarityDetector::quick_detect(const std::string& abstract, int max_distance) const {
    std::vector<uint32_t> candidates;
    
    auto tokens = TextPreprocessor::tokenize(abstract);
    tokens = TextPreprocessor::remove_stopwords(tokens);
    uint64_t input_hash = SimilarityAlgorithm::simhash(tokens);
    
    for (const auto& [pid, features] : paper_cache_) {
        int distance = SimilarityAlgorithm::simhash_distance(input_hash, features.simhash);
        if (distance <= max_distance) {
            candidates.push_back(pid);
        }
    }
    
    return candidates;
}

SimilarityResult PaperSimilarityDetector::compute_similarity(uint32_t paper_id,
                                                             const std::string& title,
                                                             const std::string& abstract,
                                                             const std::vector<std::string>& keywords) const {
    auto it = paper_cache_.find(paper_id);
    if (it == paper_cache_.end()) {
        return SimilarityResult();
    }
    
    const auto& features = it->second;
    SimilarityResult result;
    result.paper_id = paper_id;
    result.paper_title = features.title;
    
    // 处理输入
    auto title_tokens = TextPreprocessor::tokenize(title);
    title_tokens = TextPreprocessor::remove_stopwords(title_tokens);
    std::set<std::string> title_set(title_tokens.begin(), title_tokens.end());
    
    auto abstract_tokens = TextPreprocessor::tokenize(abstract);
    abstract_tokens = TextPreprocessor::remove_stopwords(abstract_tokens);
    std::set<std::string> abstract_set(abstract_tokens.begin(), abstract_tokens.end());
    
    std::set<std::string> keyword_set;
    for (const auto& kw : keywords) {
        keyword_set.insert(TextPreprocessor::to_lower(kw));
    }
    
    result.title_similarity = SimilarityAlgorithm::jaccard_similarity(title_set, features.title_tokens);
    result.abstract_similarity = SimilarityAlgorithm::jaccard_similarity(abstract_set, features.abstract_tokens);
    result.keyword_similarity = SimilarityAlgorithm::jaccard_similarity(keyword_set, features.keywords);
    
    result.overall_score = 0.3 * result.title_similarity +
                          0.5 * result.abstract_similarity +
                          0.2 * result.keyword_similarity;
    
    result.warning_level = get_warning_level(result.overall_score);
    
    return result;
}

void PaperSimilarityDetector::set_thresholds(double title, double abstract, double keyword, double overall) {
    title_threshold_ = title;
    abstract_threshold_ = abstract;
    keyword_threshold_ = keyword;
    overall_threshold_ = overall;
}

std::string PaperSimilarityDetector::get_warning_level(double similarity) {
    if (similarity >= 0.8) return "CRITICAL";
    if (similarity >= 0.6) return "HIGH";
    if (similarity >= 0.4) return "MEDIUM";
    return "LOW";
}

void PaperSimilarityDetector::rebuild_index() {
    vectorizer_ = TFIDFVectorizer();
    
    for (auto& [pid, features] : paper_cache_) {
        std::vector<std::string> tokens(features.abstract_tokens.begin(), features.abstract_tokens.end());
        vectorizer_.add_document(tokens);
    }
    
    vectorizer_.compute_idf();
}

// ==================== PlagiarismReportGenerator 实现 ====================
PlagiarismReportGenerator::FullReport PlagiarismReportGenerator::generate_report(
    uint32_t paper_id,
    const std::string& title,
    const std::string& abstract,
    const std::vector<std::string>& keywords,
    const std::vector<SimilarityResult>& results) {
    
    FullReport report;
    report.paper_id = paper_id;
    report.paper_title = title;
    report.check_time = time(nullptr);
    report.similar_papers = results;
    
    // 计算总体相似度(取最高)
    report.overall_similarity = 0;
    for (const auto& r : results) {
        if (r.overall_score > report.overall_similarity) {
            report.overall_similarity = r.overall_score;
        }
    }
    
    // 生成建议
    if (report.overall_similarity >= 0.8) {
        report.recommendation = "REJECT";
    } else if (report.overall_similarity >= 0.5) {
        report.recommendation = "REVIEW";
    } else {
        report.recommendation = "PASS";
    }
    
    return report;
}

std::string PlagiarismReportGenerator::format_text_report(const FullReport& report) {
    std::stringstream ss;
    
    ss << "╔══════════════════════════════════════════════════════════════╗\n";
    ss << "║              PAPER SIMILARITY CHECK REPORT                   ║\n";
    ss << "╠══════════════════════════════════════════════════════════════╣\n";
    
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&report.check_time));
    
    ss << "║ Paper ID: " << std::setw(10) << report.paper_id;
    ss << "                                         ║\n";
    ss << "║ Title: " << std::setw(53) << report.paper_title.substr(0, 53) << " ║\n";
    ss << "║ Check Time: " << time_buf << "                        ║\n";
    ss << "╠══════════════════════════════════════════════════════════════╣\n";
    
    ss << "║ OVERALL SIMILARITY: " << std::fixed << std::setprecision(1) 
       << (report.overall_similarity * 100) << "%";
    
    if (report.overall_similarity >= 0.8) {
        ss << "   [!!! CRITICAL !!!]";
    } else if (report.overall_similarity >= 0.6) {
        ss << "   [!! HIGH !!]";
    } else if (report.overall_similarity >= 0.4) {
        ss << "   [! MEDIUM !]";
    } else {
        ss << "   [LOW]";
    }
    ss << "                    ║\n";
    
    ss << "║ RECOMMENDATION: " << std::setw(10) << report.recommendation << "                                ║\n";
    ss << "╠══════════════════════════════════════════════════════════════╣\n";
    
    if (report.similar_papers.empty()) {
        ss << "║ No similar papers found.                                     ║\n";
    } else {
        ss << "║ SIMILAR PAPERS FOUND: " << report.similar_papers.size() << "                                      ║\n";
        ss << "╠──────────────────────────────────────────────────────────────╣\n";
        
        int count = 0;
        for (const auto& sim : report.similar_papers) {
            if (++count > 5) break;  // 只显示前5个
            
            ss << "║ #" << count << " Paper " << sim.paper_id << ": " 
               << sim.paper_title.substr(0, 40) << "...\n";
            ss << "║    Overall: " << std::fixed << std::setprecision(1) << (sim.overall_score * 100) << "% | ";
            ss << "Title: " << (sim.title_similarity * 100) << "% | ";
            ss << "Abstract: " << (sim.abstract_similarity * 100) << "%\n";
            ss << "╠──────────────────────────────────────────────────────────────╣\n";
        }
    }
    
    ss << "╚══════════════════════════════════════════════════════════════╝\n";
    
    return ss.str();
}

std::string PlagiarismReportGenerator::format_json_report(const FullReport& report) {
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"paper_id\": " << report.paper_id << ",\n";
    ss << "  \"paper_title\": \"" << report.paper_title << "\",\n";
    ss << "  \"check_time\": " << report.check_time << ",\n";
    ss << "  \"overall_similarity\": " << std::fixed << std::setprecision(4) << report.overall_similarity << ",\n";
    ss << "  \"recommendation\": \"" << report.recommendation << "\",\n";
    ss << "  \"similar_papers\": [\n";
    
    for (size_t i = 0; i < report.similar_papers.size(); ++i) {
        const auto& sim = report.similar_papers[i];
        ss << "    {\n";
        ss << "      \"paper_id\": " << sim.paper_id << ",\n";
        ss << "      \"title\": \"" << sim.paper_title << "\",\n";
        ss << "      \"overall_score\": " << sim.overall_score << ",\n";
        ss << "      \"title_similarity\": " << sim.title_similarity << ",\n";
        ss << "      \"abstract_similarity\": " << sim.abstract_similarity << ",\n";
        ss << "      \"keyword_similarity\": " << sim.keyword_similarity << ",\n";
        ss << "      \"warning_level\": \"" << sim.warning_level << "\"\n";
        ss << "    }";
        if (i < report.similar_papers.size() - 1) ss << ",";
        ss << "\n";
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string PlagiarismReportGenerator::format_colored_report(const FullReport& report) {
    std::stringstream ss;
    
    // 使用ANSI颜色代码
    const char* RESET = "\033[0m";
    const char* BOLD = "\033[1m";
    const char* RED = "\033[31m";
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* CYAN = "\033[36m";
    
    ss << BOLD << CYAN << "═══════════════════════════════════════════════════════════" << RESET << "\n";
    ss << BOLD << "           PAPER SIMILARITY CHECK REPORT" << RESET << "\n";
    ss << CYAN << "═══════════════════════════════════════════════════════════" << RESET << "\n";
    
    ss << "Paper ID: " << BOLD << report.paper_id << RESET << "\n";
    ss << "Title: " << report.paper_title << "\n\n";
    
    // 相似度显示
    ss << "Overall Similarity: " << BOLD;
    if (report.overall_similarity >= 0.8) {
        ss << RED << std::fixed << std::setprecision(1) << (report.overall_similarity * 100) << "%" 
           << " [CRITICAL]" << RESET;
    } else if (report.overall_similarity >= 0.5) {
        ss << YELLOW << (report.overall_similarity * 100) << "%" << " [HIGH]" << RESET;
    } else {
        ss << GREEN << (report.overall_similarity * 100) << "%" << " [LOW]" << RESET;
    }
    ss << "\n";
    
    ss << "Recommendation: " << BOLD;
    if (report.recommendation == "REJECT") {
        ss << RED << "REJECT" << RESET;
    } else if (report.recommendation == "REVIEW") {
        ss << YELLOW << "MANUAL REVIEW REQUIRED" << RESET;
    } else {
        ss << GREEN << "PASS" << RESET;
    }
    ss << "\n\n";
    
    if (!report.similar_papers.empty()) {
        ss << CYAN << "Similar Papers Found:" << RESET << "\n";
        for (const auto& sim : report.similar_papers) {
            ss << "  • Paper " << sim.paper_id << " (" << sim.paper_title.substr(0, 30) << "...)\n";
            ss << "    Similarity: " << std::fixed << std::setprecision(1) << (sim.overall_score * 100) << "%\n";
        }
    }
    
    ss << CYAN << "═══════════════════════════════════════════════════════════" << RESET << "\n";
    
    return ss.str();
}
