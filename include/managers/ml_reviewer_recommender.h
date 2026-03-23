#ifndef ML_REVIEWER_RECOMMENDER_H
#define ML_REVIEWER_RECOMMENDER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <cmath>
#include <algorithm>
#include <random>
#include <numeric>
#include <unordered_map>
#include <functional>
#include <queue>

// ==================== 特征向量 ====================
class FeatureVector {
public:
    std::vector<double> values;
    
    FeatureVector() = default;
    explicit FeatureVector(size_t size) : values(size, 0.0) {}
    explicit FeatureVector(const std::vector<double>& v) : values(v) {}
    
    size_t size() const { return values.size(); }
    
    double& operator[](size_t i) { return values[i]; }
    const double& operator[](size_t i) const { return values[i]; }
    
    // 向量运算
    FeatureVector operator+(const FeatureVector& other) const {
        FeatureVector result(size());
        for (size_t i = 0; i < size(); i++) {
            result[i] = values[i] + other.values[i];
        }
        return result;
    }
    
    FeatureVector operator-(const FeatureVector& other) const {
        FeatureVector result(size());
        for (size_t i = 0; i < size(); i++) {
            result[i] = values[i] - other.values[i];
        }
        return result;
    }
    
    FeatureVector operator*(double scalar) const {
        FeatureVector result(size());
        for (size_t i = 0; i < size(); i++) {
            result[i] = values[i] * scalar;
        }
        return result;
    }
    
    // 点积
    double dot(const FeatureVector& other) const {
        double sum = 0.0;
        for (size_t i = 0; i < size() && i < other.size(); i++) {
            sum += values[i] * other.values[i];
        }
        return sum;
    }
    
    // L2范数
    double norm() const {
        return std::sqrt(dot(*this));
    }
    
    // 余弦相似度
    double cosineSimilarity(const FeatureVector& other) const {
        double n1 = norm();
        double n2 = other.norm();
        if (n1 < 1e-10 || n2 < 1e-10) return 0.0;
        return dot(other) / (n1 * n2);
    }
    
    // 归一化
    void normalize() {
        double n = norm();
        if (n > 1e-10) {
            for (auto& v : values) {
                v /= n;
            }
        }
    }
};

// ==================== 词嵌入模型(Word2Vec简化版) ====================
class WordEmbedding {
private:
    std::unordered_map<std::string, FeatureVector> word_vectors_;
    size_t embedding_dim_;
    std::mt19937 rng_;
    
public:
    explicit WordEmbedding(size_t dim = 100) : embedding_dim_(dim), rng_(42) {}
    
    // 训练词嵌入(简化版CBOW)
    void train(const std::vector<std::vector<std::string>>& corpus, 
               int window_size = 5, int epochs = 5, double learning_rate = 0.01) {
        
        // 构建词汇表
        std::set<std::string> vocab;
        for (const auto& doc : corpus) {
            for (const auto& word : doc) {
                vocab.insert(word);
            }
        }
        
        // 初始化词向量
        std::normal_distribution<double> dist(0.0, 0.1);
        for (const auto& word : vocab) {
            FeatureVector vec(embedding_dim_);
            for (size_t i = 0; i < embedding_dim_; i++) {
                vec[i] = dist(rng_);
            }
            word_vectors_[word] = vec;
        }
        
        // 训练
        for (int epoch = 0; epoch < epochs; epoch++) {
            for (const auto& doc : corpus) {
                for (size_t i = 0; i < doc.size(); i++) {
                    // 获取上下文词
                    std::vector<std::string> context;
                    for (int j = -window_size; j <= window_size; j++) {
                        if (j != 0) {
                            int idx = static_cast<int>(i) + j;
                            if (idx >= 0 && idx < static_cast<int>(doc.size())) {
                                context.push_back(doc[idx]);
                            }
                        }
                    }
                    
                    if (context.empty()) continue;
                    
                    // 计算上下文平均向量
                    FeatureVector context_avg(embedding_dim_);
                    for (const auto& ctx_word : context) {
                        auto it = word_vectors_.find(ctx_word);
                        if (it != word_vectors_.end()) {
                            context_avg = context_avg + it->second;
                        }
                    }
                    context_avg = context_avg * (1.0 / context.size());
                    
                    // 更新目标词向量
                    const std::string& target = doc[i];
                    auto& target_vec = word_vectors_[target];
                    
                    // 简化的梯度更新
                    FeatureVector grad = context_avg - target_vec;
                    target_vec = target_vec + grad * learning_rate;
                }
            }
        }
    }
    
    // 获取词向量
    FeatureVector getWordVector(const std::string& word) const {
        auto it = word_vectors_.find(word);
        if (it != word_vectors_.end()) {
            return it->second;
        }
        return FeatureVector(embedding_dim_);
    }
    
    // 获取文档向量(词向量平均)
    FeatureVector getDocumentVector(const std::vector<std::string>& words) const {
        FeatureVector doc_vec(embedding_dim_);
        int count = 0;
        
        for (const auto& word : words) {
            auto it = word_vectors_.find(word);
            if (it != word_vectors_.end()) {
                doc_vec = doc_vec + it->second;
                count++;
            }
        }
        
        if (count > 0) {
            doc_vec = doc_vec * (1.0 / count);
        }
        
        return doc_vec;
    }
    
    // 查找相似词
    std::vector<std::pair<std::string, double>> findSimilarWords(
            const std::string& word, size_t top_k = 10) const {
        
        auto it = word_vectors_.find(word);
        if (it == word_vectors_.end()) {
            return {};
        }
        
        const FeatureVector& target = it->second;
        std::vector<std::pair<std::string, double>> similarities;
        
        for (const auto& [w, vec] : word_vectors_) {
            if (w != word) {
                double sim = target.cosineSimilarity(vec);
                similarities.push_back({w, sim});
            }
        }
        
        std::sort(similarities.begin(), similarities.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (similarities.size() > top_k) {
            similarities.resize(top_k);
        }
        
        return similarities;
    }
    
    size_t getDimension() const { return embedding_dim_; }
    size_t getVocabSize() const { return word_vectors_.size(); }
};

// ==================== TF-IDF特征提取器 ====================
class TFIDFFeatureExtractor {
private:
    std::unordered_map<std::string, int> doc_freq_;      // 文档频率
    std::unordered_map<std::string, int> word_to_idx_;   // 词到索引映射
    std::vector<std::string> idx_to_word_;               // 索引到词映射
    int total_docs_;
    
public:
    TFIDFFeatureExtractor() : total_docs_(0) {}
    
    // 从语料库学习词汇表
    void fit(const std::vector<std::vector<std::string>>& corpus, int min_df = 2, int max_vocab = 10000) {
        doc_freq_.clear();
        word_to_idx_.clear();
        idx_to_word_.clear();
        total_docs_ = corpus.size();
        
        // 统计文档频率
        for (const auto& doc : corpus) {
            std::set<std::string> unique_words(doc.begin(), doc.end());
            for (const auto& word : unique_words) {
                doc_freq_[word]++;
            }
        }
        
        // 过滤并排序
        std::vector<std::pair<std::string, int>> sorted_words;
        for (const auto& [word, freq] : doc_freq_) {
            if (freq >= min_df) {
                sorted_words.push_back({word, freq});
            }
        }
        
        std::sort(sorted_words.begin(), sorted_words.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // 构建词汇表
        for (size_t i = 0; i < sorted_words.size() && static_cast<int>(i) < max_vocab; i++) {
            word_to_idx_[sorted_words[i].first] = i;
            idx_to_word_.push_back(sorted_words[i].first);
        }
    }
    
    // 转换文档为TF-IDF向量
    FeatureVector transform(const std::vector<std::string>& doc) const {
        FeatureVector vec(idx_to_word_.size());
        
        // 计算词频
        std::unordered_map<std::string, int> tf;
        for (const auto& word : doc) {
            tf[word]++;
        }
        
        // 计算TF-IDF
        for (const auto& [word, freq] : tf) {
            auto it = word_to_idx_.find(word);
            if (it != word_to_idx_.end()) {
                int idx = it->second;
                double tf_val = 1.0 + std::log(freq);
                double idf_val = std::log(1.0 + total_docs_ / (1.0 + doc_freq_.at(word)));
                vec[idx] = tf_val * idf_val;
            }
        }
        
        vec.normalize();
        return vec;
    }
    
    size_t getVocabSize() const { return idx_to_word_.size(); }
};

// ==================== K近邻分类器 ====================
class KNNClassifier {
private:
    std::vector<std::pair<FeatureVector, int>> training_data_;
    int k_;
    
public:
    explicit KNNClassifier(int k = 5) : k_(k) {}
    
    void fit(const std::vector<FeatureVector>& X, const std::vector<int>& y) {
        training_data_.clear();
        for (size_t i = 0; i < X.size(); i++) {
            training_data_.push_back({X[i], y[i]});
        }
    }
    
    int predict(const FeatureVector& x) const {
        // 计算距离
        std::vector<std::pair<double, int>> distances;
        for (const auto& [vec, label] : training_data_) {
            double sim = x.cosineSimilarity(vec);
            distances.push_back({sim, label});
        }
        
        // 排序找k近邻
        std::sort(distances.begin(), distances.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // 投票
        std::map<int, double> votes;
        for (int i = 0; i < k_ && i < static_cast<int>(distances.size()); i++) {
            votes[distances[i].second] += distances[i].first;  // 加权投票
        }
        
        int best_label = -1;
        double best_score = -1;
        for (const auto& [label, score] : votes) {
            if (score > best_score) {
                best_score = score;
                best_label = label;
            }
        }
        
        return best_label;
    }
    
    // 获取概率分布
    std::map<int, double> predictProba(const FeatureVector& x) const {
        std::vector<std::pair<double, int>> distances;
        for (const auto& [vec, label] : training_data_) {
            double sim = x.cosineSimilarity(vec);
            distances.push_back({sim, label});
        }
        
        std::sort(distances.begin(), distances.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        std::map<int, double> proba;
        double total = 0.0;
        
        for (int i = 0; i < k_ && i < static_cast<int>(distances.size()); i++) {
            proba[distances[i].second] += distances[i].first;
            total += distances[i].first;
        }
        
        if (total > 0) {
            for (auto& [label, score] : proba) {
                score /= total;
            }
        }
        
        return proba;
    }
};

// ==================== 协同过滤推荐器 ====================
class CollaborativeFilter {
private:
    // 用户-物品评分矩阵
    std::map<uint32_t, std::map<uint32_t, double>> user_item_ratings_;
    // 物品-用户评分矩阵(转置)
    std::map<uint32_t, std::map<uint32_t, double>> item_user_ratings_;
    
    // 用户相似度缓存
    std::map<std::pair<uint32_t, uint32_t>, double> user_similarity_cache_;
    
public:
    // 添加评分
    void addRating(uint32_t user_id, uint32_t item_id, double rating) {
        user_item_ratings_[user_id][item_id] = rating;
        item_user_ratings_[item_id][user_id] = rating;
        user_similarity_cache_.clear();  // 清除缓存
    }
    
    // 批量添加评分
    void addRatings(const std::vector<std::tuple<uint32_t, uint32_t, double>>& ratings) {
        for (const auto& [user, item, rating] : ratings) {
            user_item_ratings_[user][item] = rating;
            item_user_ratings_[item][user] = rating;
        }
        user_similarity_cache_.clear();
    }
    
    // 基于用户的协同过滤推荐
    std::vector<std::pair<uint32_t, double>> recommendForUser(uint32_t user_id, size_t top_k = 10) {
        if (user_item_ratings_.find(user_id) == user_item_ratings_.end()) {
            return {};
        }
        
        const auto& user_ratings = user_item_ratings_[user_id];
        
        // 找相似用户
        std::vector<std::pair<uint32_t, double>> similar_users;
        for (const auto& [other_user, _] : user_item_ratings_) {
            if (other_user != user_id) {
                double sim = computeUserSimilarity(user_id, other_user);
                if (sim > 0.1) {
                    similar_users.push_back({other_user, sim});
                }
            }
        }
        
        std::sort(similar_users.begin(), similar_users.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // 预测评分
        std::map<uint32_t, double> score_sum;
        std::map<uint32_t, double> weight_sum;
        
        for (size_t i = 0; i < similar_users.size() && i < 50; i++) {
            uint32_t sim_user = similar_users[i].first;
            double sim = similar_users[i].second;
            
            for (const auto& [item, rating] : user_item_ratings_[sim_user]) {
                if (user_ratings.find(item) == user_ratings.end()) {
                    score_sum[item] += sim * rating;
                    weight_sum[item] += sim;
                }
            }
        }
        
        // 生成推荐列表
        std::vector<std::pair<uint32_t, double>> recommendations;
        for (const auto& [item, score] : score_sum) {
            if (weight_sum[item] > 0) {
                recommendations.push_back({item, score / weight_sum[item]});
            }
        }
        
        std::sort(recommendations.begin(), recommendations.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (recommendations.size() > top_k) {
            recommendations.resize(top_k);
        }
        
        return recommendations;
    }
    
    // 基于物品的协同过滤
    std::vector<std::pair<uint32_t, double>> recommendSimilarItems(uint32_t item_id, size_t top_k = 10) {
        if (item_user_ratings_.find(item_id) == item_user_ratings_.end()) {
            return {};
        }
        
        std::vector<std::pair<uint32_t, double>> similar_items;
        
        for (const auto& [other_item, _] : item_user_ratings_) {
            if (other_item != item_id) {
                double sim = computeItemSimilarity(item_id, other_item);
                if (sim > 0.1) {
                    similar_items.push_back({other_item, sim});
                }
            }
        }
        
        std::sort(similar_items.begin(), similar_items.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (similar_items.size() > top_k) {
            similar_items.resize(top_k);
        }
        
        return similar_items;
    }
    
private:
    double computeUserSimilarity(uint32_t user1, uint32_t user2) {
        auto key = std::make_pair(std::min(user1, user2), std::max(user1, user2));
        auto it = user_similarity_cache_.find(key);
        if (it != user_similarity_cache_.end()) {
            return it->second;
        }
        
        const auto& ratings1 = user_item_ratings_[user1];
        const auto& ratings2 = user_item_ratings_[user2];
        
        // 找共同评分的物品
        std::vector<double> v1, v2;
        for (const auto& [item, r1] : ratings1) {
            auto it = ratings2.find(item);
            if (it != ratings2.end()) {
                v1.push_back(r1);
                v2.push_back(it->second);
            }
        }
        
        if (v1.size() < 2) {
            user_similarity_cache_[key] = 0.0;
            return 0.0;
        }
        
        // Pearson相关系数
        double mean1 = std::accumulate(v1.begin(), v1.end(), 0.0) / v1.size();
        double mean2 = std::accumulate(v2.begin(), v2.end(), 0.0) / v2.size();
        
        double num = 0.0, den1 = 0.0, den2 = 0.0;
        for (size_t i = 0; i < v1.size(); i++) {
            double d1 = v1[i] - mean1;
            double d2 = v2[i] - mean2;
            num += d1 * d2;
            den1 += d1 * d1;
            den2 += d2 * d2;
        }
        
        double denom = std::sqrt(den1 * den2);
        double sim = denom > 1e-10 ? num / denom : 0.0;
        
        user_similarity_cache_[key] = sim;
        return sim;
    }
    
    double computeItemSimilarity(uint32_t item1, uint32_t item2) {
        const auto& users1 = item_user_ratings_[item1];
        const auto& users2 = item_user_ratings_[item2];
        
        std::vector<double> v1, v2;
        for (const auto& [user, r1] : users1) {
            auto it = users2.find(user);
            if (it != users2.end()) {
                v1.push_back(r1);
                v2.push_back(it->second);
            }
        }
        
        if (v1.size() < 2) return 0.0;
        
        // 余弦相似度
        double dot = 0.0, norm1 = 0.0, norm2 = 0.0;
        for (size_t i = 0; i < v1.size(); i++) {
            dot += v1[i] * v2[i];
            norm1 += v1[i] * v1[i];
            norm2 += v2[i] * v2[i];
        }
        
        double denom = std::sqrt(norm1 * norm2);
        return denom > 1e-10 ? dot / denom : 0.0;
    }
};

// ==================== 审稿人特征 ====================
struct ReviewerFeatures {
    uint32_t reviewer_id;
    std::string name;
    std::vector<std::string> research_areas;
    std::vector<std::string> keywords;
    std::vector<std::string> publications;
    
    FeatureVector embedding;         // 专业领域嵌入
    FeatureVector tfidf_vector;      // TF-IDF特征
    
    int total_reviews;               // 总审稿数
    int pending_reviews;             // 待审稿数
    double avg_review_score;         // 平均评分质量
    double avg_review_time_days;     // 平均审稿时间
    double response_rate;            // 响应率
    
    std::map<std::string, int> area_experience;  // 各领域审稿经验
    
    ReviewerFeatures() : reviewer_id(0), total_reviews(0), pending_reviews(0),
                         avg_review_score(0), avg_review_time_days(0), response_rate(1.0) {}
};

// ==================== 论文特征 ====================
struct PaperFeatures {
    uint32_t paper_id;
    std::string title;
    std::string abstract;
    std::vector<std::string> keywords;
    std::string research_area;
    std::vector<uint32_t> author_ids;
    
    FeatureVector embedding;
    FeatureVector tfidf_vector;
    
    PaperFeatures() : paper_id(0) {}
};

// ==================== ML审稿人推荐器 ====================
class MLReviewerRecommender {
public:
    struct RecommendationResult {
        uint32_t reviewer_id;
        std::string reviewer_name;
        double overall_score;          // 综合推荐分数
        double expertise_score;        // 专业匹配度
        double availability_score;     // 可用性分数
        double historical_score;       // 历史表现分数
        double collaborative_score;    // 协同过滤分数
        std::string explanation;
        std::vector<std::string> matched_keywords;
        bool has_coi;                  // 利益冲突
        
        RecommendationResult() : reviewer_id(0), overall_score(0), expertise_score(0),
                                 availability_score(0), historical_score(0),
                                 collaborative_score(0), has_coi(false) {}
    };
    
private:
    WordEmbedding word_embedding_;
    TFIDFFeatureExtractor tfidf_extractor_;
    CollaborativeFilter collab_filter_;
    
    std::map<uint32_t, ReviewerFeatures> reviewers_;
    std::map<uint32_t, PaperFeatures> papers_;
    
    // 历史审稿记录: (reviewer_id, paper_id) -> score
    std::map<std::pair<uint32_t, uint32_t>, double> review_history_;
    
    // 权重配置
    double weight_expertise_ = 0.4;
    double weight_availability_ = 0.2;
    double weight_historical_ = 0.2;
    double weight_collaborative_ = 0.2;
    
    // 约束配置
    int max_pending_reviews_ = 5;
    
public:
    MLReviewerRecommender() : word_embedding_(100) {}
    
    // 训练模型
    void train(const std::vector<std::vector<std::string>>& text_corpus) {
        // 训练词嵌入
        word_embedding_.train(text_corpus, 5, 10, 0.025);
        
        // 训练TF-IDF
        tfidf_extractor_.fit(text_corpus);
        
        // 更新所有审稿人和论文的特征向量
        updateAllFeatureVectors();
    }
    
    // 添加审稿人
    void addReviewer(const ReviewerFeatures& reviewer) {
        reviewers_[reviewer.reviewer_id] = reviewer;
        updateReviewerFeatures(reviewer.reviewer_id);
    }
    
    // 添加论文
    void addPaper(const PaperFeatures& paper) {
        papers_[paper.paper_id] = paper;
        updatePaperFeatures(paper.paper_id);
    }
    
    // 记录审稿历史
    void recordReview(uint32_t reviewer_id, uint32_t paper_id, double quality_score) {
        review_history_[{reviewer_id, paper_id}] = quality_score;
        
        // 更新协同过滤
        // 将审稿质量作为"评分"
        collab_filter_.addRating(reviewer_id, paper_id, quality_score);
        
        // 更新审稿人统计
        if (reviewers_.find(reviewer_id) != reviewers_.end()) {
            auto& r = reviewers_[reviewer_id];
            double n = r.total_reviews;
            r.avg_review_score = (r.avg_review_score * n + quality_score) / (n + 1);
            r.total_reviews++;
        }
    }
    
    // 推荐审稿人
    std::vector<RecommendationResult> recommend(uint32_t paper_id, size_t top_k = 5,
                                                const std::vector<uint32_t>& exclude_ids = {}) {
        std::vector<RecommendationResult> results;
        
        auto paper_it = papers_.find(paper_id);
        if (paper_it == papers_.end()) {
            return results;
        }
        
        const PaperFeatures& paper = paper_it->second;
        std::set<uint32_t> exclude_set(exclude_ids.begin(), exclude_ids.end());
        
        // 添加作者到排除列表(COI)
        for (uint32_t author_id : paper.author_ids) {
            exclude_set.insert(author_id);
        }
        
        // 计算每个审稿人的分数
        for (const auto& [reviewer_id, reviewer] : reviewers_) {
            if (exclude_set.count(reviewer_id)) continue;
            
            RecommendationResult result;
            result.reviewer_id = reviewer_id;
            result.reviewer_name = reviewer.name;
            
            // 1. 专业匹配度
            result.expertise_score = computeExpertiseScore(paper, reviewer);
            result.matched_keywords = findMatchedKeywords(paper, reviewer);
            
            // 2. 可用性分数
            result.availability_score = computeAvailabilityScore(reviewer);
            
            // 3. 历史表现分数
            result.historical_score = computeHistoricalScore(reviewer);
            
            // 4. 协同过滤分数
            result.collaborative_score = computeCollaborativeScore(reviewer_id, paper_id);
            
            // 5. COI检测
            result.has_coi = detectCOI(paper, reviewer);
            
            // 计算综合分数
            if (result.has_coi) {
                result.overall_score = 0;
                result.explanation = "Conflict of interest detected";
            } else {
                result.overall_score = 
                    weight_expertise_ * result.expertise_score +
                    weight_availability_ * result.availability_score +
                    weight_historical_ * result.historical_score +
                    weight_collaborative_ * result.collaborative_score;
                
                result.explanation = generateExplanation(result);
            }
            
            if (result.overall_score > 0.1 || result.matched_keywords.size() > 0) {
                results.push_back(result);
            }
        }
        
        // 排序
        std::sort(results.begin(), results.end(),
                  [](const RecommendationResult& a, const RecommendationResult& b) {
                      return a.overall_score > b.overall_score;
                  });
        
        // 截取top_k
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }
    
    // 基于文本的快速推荐(不需要预先添加Paper)
    std::vector<RecommendationResult> recommendByText(const std::string& title,
                                                       const std::string& abstract,
                                                       const std::vector<std::string>& keywords,
                                                       size_t top_k = 5) {
        // 创建临时论文特征
        PaperFeatures paper;
        paper.paper_id = 0;
        paper.title = title;
        paper.abstract = abstract;
        paper.keywords = keywords;
        
        // 计算特征向量
        std::vector<std::string> all_text;
        for (const auto& word : tokenize(title + " " + abstract)) {
            all_text.push_back(word);
        }
        for (const auto& kw : keywords) {
            all_text.push_back(kw);
        }
        
        paper.embedding = word_embedding_.getDocumentVector(all_text);
        paper.tfidf_vector = tfidf_extractor_.transform(all_text);
        
        // 推荐
        std::vector<RecommendationResult> results;
        
        for (const auto& [reviewer_id, reviewer] : reviewers_) {
            RecommendationResult result;
            result.reviewer_id = reviewer_id;
            result.reviewer_name = reviewer.name;
            
            result.expertise_score = computeExpertiseScore(paper, reviewer);
            result.matched_keywords = findMatchedKeywords(paper, reviewer);
            result.availability_score = computeAvailabilityScore(reviewer);
            result.historical_score = computeHistoricalScore(reviewer);
            result.collaborative_score = 0.5;  // 默认中等
            
            result.overall_score = 
                weight_expertise_ * result.expertise_score +
                weight_availability_ * result.availability_score +
                weight_historical_ * result.historical_score +
                weight_collaborative_ * result.collaborative_score;
            
            result.explanation = generateExplanation(result);
            
            if (result.overall_score > 0.1) {
                results.push_back(result);
            }
        }
        
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.overall_score > b.overall_score; });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }
    
    // 设置权重
    void setWeights(double expertise, double availability, double historical, double collaborative) {
        double sum = expertise + availability + historical + collaborative;
        weight_expertise_ = expertise / sum;
        weight_availability_ = availability / sum;
        weight_historical_ = historical / sum;
        weight_collaborative_ = collaborative / sum;
    }
    
    // 获取审稿人数量
    size_t getReviewerCount() const { return reviewers_.size(); }
    
    // 获取论文数量
    size_t getPaperCount() const { return papers_.size(); }
    
private:
    void updateAllFeatureVectors() {
        for (auto& [id, reviewer] : reviewers_) {
            updateReviewerFeatures(id);
        }
        for (auto& [id, paper] : papers_) {
            updatePaperFeatures(id);
        }
    }
    
    void updateReviewerFeatures(uint32_t reviewer_id) {
        auto it = reviewers_.find(reviewer_id);
        if (it == reviewers_.end()) return;
        
        ReviewerFeatures& r = it->second;
        
        // 构建文本
        std::vector<std::string> all_text;
        for (const auto& area : r.research_areas) {
            for (const auto& word : tokenize(area)) {
                all_text.push_back(word);
            }
        }
        for (const auto& kw : r.keywords) {
            all_text.push_back(kw);
        }
        
        r.embedding = word_embedding_.getDocumentVector(all_text);
        r.tfidf_vector = tfidf_extractor_.transform(all_text);
    }
    
    void updatePaperFeatures(uint32_t paper_id) {
        auto it = papers_.find(paper_id);
        if (it == papers_.end()) return;
        
        PaperFeatures& p = it->second;
        
        std::vector<std::string> all_text;
        for (const auto& word : tokenize(p.title + " " + p.abstract)) {
            all_text.push_back(word);
        }
        for (const auto& kw : p.keywords) {
            all_text.push_back(kw);
        }
        
        p.embedding = word_embedding_.getDocumentVector(all_text);
        p.tfidf_vector = tfidf_extractor_.transform(all_text);
    }
    
    double computeExpertiseScore(const PaperFeatures& paper, const ReviewerFeatures& reviewer) {
        // 1. 嵌入向量相似度
        double embedding_sim = paper.embedding.cosineSimilarity(reviewer.embedding);
        
        // 2. TF-IDF相似度
        double tfidf_sim = paper.tfidf_vector.cosineSimilarity(reviewer.tfidf_vector);
        
        // 3. 关键词匹配
        double keyword_match = computeKeywordMatch(paper.keywords, reviewer.keywords);
        
        // 4. 研究领域匹配
        double area_match = 0.0;
        for (const auto& area : reviewer.research_areas) {
            if (paper.research_area.find(area) != std::string::npos ||
                area.find(paper.research_area) != std::string::npos) {
                area_match = 1.0;
                break;
            }
        }
        
        return 0.3 * embedding_sim + 0.3 * tfidf_sim + 0.2 * keyword_match + 0.2 * area_match;
    }
    
    double computeKeywordMatch(const std::vector<std::string>& paper_kw,
                               const std::vector<std::string>& reviewer_kw) {
        if (paper_kw.empty() || reviewer_kw.empty()) return 0.0;
        
        std::set<std::string> paper_set, reviewer_set;
        for (const auto& kw : paper_kw) {
            paper_set.insert(toLowerCase(kw));
        }
        for (const auto& kw : reviewer_kw) {
            reviewer_set.insert(toLowerCase(kw));
        }
        
        std::vector<std::string> intersection;
        std::set_intersection(paper_set.begin(), paper_set.end(),
                              reviewer_set.begin(), reviewer_set.end(),
                              std::back_inserter(intersection));
        
        return static_cast<double>(intersection.size()) / paper_set.size();
    }
    
    std::vector<std::string> findMatchedKeywords(const PaperFeatures& paper,
                                                   const ReviewerFeatures& reviewer) {
        std::vector<std::string> matched;
        
        std::set<std::string> reviewer_kw_set;
        for (const auto& kw : reviewer.keywords) {
            reviewer_kw_set.insert(toLowerCase(kw));
        }
        for (const auto& area : reviewer.research_areas) {
            reviewer_kw_set.insert(toLowerCase(area));
        }
        
        for (const auto& kw : paper.keywords) {
            if (reviewer_kw_set.count(toLowerCase(kw))) {
                matched.push_back(kw);
            }
        }
        
        return matched;
    }
    
    double computeAvailabilityScore(const ReviewerFeatures& reviewer) {
        // 基于当前待审稿数计算可用性
        if (reviewer.pending_reviews >= max_pending_reviews_) {
            return 0.0;
        }
        
        double load_factor = 1.0 - static_cast<double>(reviewer.pending_reviews) / max_pending_reviews_;
        return load_factor * reviewer.response_rate;
    }
    
    double computeHistoricalScore(const ReviewerFeatures& reviewer) {
        if (reviewer.total_reviews == 0) {
            return 0.5;  // 新审稿人默认中等分数
        }
        
        // 综合评审质量和及时性
        double quality = reviewer.avg_review_score / 10.0;  // 假设满分10
        double timeliness = std::max(0.0, 1.0 - reviewer.avg_review_time_days / 30.0);
        
        return 0.7 * quality + 0.3 * timeliness;
    }
    
    double computeCollaborativeScore(uint32_t reviewer_id, uint32_t paper_id) {
        // 使用协同过滤预测审稿人对该论文的适合度
        auto recommendations = collab_filter_.recommendForUser(reviewer_id, 100);
        
        for (const auto& [item, score] : recommendations) {
            if (item == paper_id) {
                return score / 10.0;  // 归一化
            }
        }
        
        return 0.5;  // 默认中等分数
    }
    
    bool detectCOI(const PaperFeatures& paper, const ReviewerFeatures& reviewer) {
        // 检查作者-审稿人冲突
        for (uint32_t author_id : paper.author_ids) {
            if (author_id == reviewer.reviewer_id) {
                return true;
            }
        }
        
        // 这里可以添加更多COI检测逻辑
        // 如：同单位、近期合作等
        
        return false;
    }
    
    std::string generateExplanation(const RecommendationResult& result) {
        std::stringstream ss;
        
        ss << "Score breakdown: ";
        ss << "Expertise=" << std::fixed << std::setprecision(2) << result.expertise_score << ", ";
        ss << "Availability=" << result.availability_score << ", ";
        ss << "Historical=" << result.historical_score;
        
        if (!result.matched_keywords.empty()) {
            ss << ". Matched keywords: ";
            for (size_t i = 0; i < result.matched_keywords.size() && i < 3; i++) {
                if (i > 0) ss << ", ";
                ss << result.matched_keywords[i];
            }
        }
        
        return ss.str();
    }
    
    std::vector<std::string> tokenize(const std::string& text) {
        std::vector<std::string> tokens;
        std::string word;
        
        for (char c : text) {
            if (std::isalnum(c)) {
                word += std::tolower(c);
            } else if (!word.empty()) {
                if (word.size() >= 2) {
                    tokens.push_back(word);
                }
                word.clear();
            }
        }
        
        if (!word.empty() && word.size() >= 2) {
            tokens.push_back(word);
        }
        
        return tokens;
    }
    
    std::string toLowerCase(const std::string& str) {
        std::string result;
        for (char c : str) {
            result += std::tolower(c);
        }
        return result;
    }
};

#endif // ML_REVIEWER_RECOMMENDER_H
