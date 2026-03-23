#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <vector>
#include <map>
#include <list>
#include <mutex>
#include <memory>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <cstring>

// ==================== 常量定义 ====================
constexpr size_t BLOCK_SIZE = 4096;              // 块大小 4KB
constexpr size_t INODE_SIZE = 256;               // INode大小
constexpr size_t MAX_BLOCKS = 65536;             // 最大块数 256MB
constexpr size_t MAX_INODES = 8192;              // 最大INode数
constexpr size_t DIRECT_BLOCKS = 12;             // 直接块指针数
constexpr size_t INDIRECT_BLOCKS = 1;            // 一级间接块指针数
constexpr size_t MAX_FILENAME = 255;             // 最大文件名长度
constexpr size_t MAX_PATH = 4096;                // 最大路径长度
constexpr size_t DEFAULT_CACHE_SIZE = 128;       // 默认缓存块数

// ==================== 文件类型 ====================
enum class FileType : uint8_t {
    REGULAR_FILE = 1,
    DIRECTORY = 2,
    SYMLINK = 3
};

// ==================== 超级块结构 ====================
struct SuperBlock {
    uint32_t magic;                    // 魔数 0x52455649 ("REVI")
    uint32_t version;                  // 文件系统版本
    uint64_t total_blocks;             // 总块数
    uint64_t total_inodes;             // 总INode数
    uint64_t free_blocks;              // 空闲块数
    uint64_t free_inodes;              // 空闲INode数
    uint64_t first_data_block;         // 第一个数据块号
    uint64_t inode_table_block;        // INode表起始块号
    uint64_t bitmap_block;             // 位图起始块号
    uint64_t root_inode;               // 根目录INode号
    time_t create_time;                // 创建时间
    time_t mount_time;                 // 挂载时间
    uint32_t mount_count;              // 挂载次数
    uint32_t max_mount_count;          // 最大挂载次数
    uint32_t block_size;               // 块大小
    uint32_t inode_size;               // INode大小
    char volume_name[64];              // 卷名
    uint8_t reserved[128];             // 保留字段
    
    SuperBlock() : magic(0x52455649), version(1), total_blocks(MAX_BLOCKS),
                   total_inodes(MAX_INODES), free_blocks(MAX_BLOCKS - 1),
                   free_inodes(MAX_INODES - 1), first_data_block(0),
                   inode_table_block(1), bitmap_block(0), root_inode(0),
                   mount_count(0), max_mount_count(100),
                   block_size(BLOCK_SIZE), inode_size(INODE_SIZE) {
        create_time = mount_time = time(nullptr);
        strcpy(volume_name, "ReviewFS");
    }
};

// ==================== INode结构 ====================
struct INode {
    uint32_t inode_number;                      // INode编号
    FileType file_type;                         // 文件类型
    uint32_t permissions;                       // 权限位
    uint32_t uid;                               // 用户ID
    uint32_t gid;                               // 组ID
    uint64_t file_size;                         // 文件大小
    time_t create_time;                         // 创建时间
    time_t modify_time;                         // 修改时间
    time_t access_time;                         // 访问时间
    uint32_t links_count;                       // 硬链接计数
    uint64_t blocks_count;                      // 占用的块数
    uint64_t direct_blocks[DIRECT_BLOCKS];     // 直接块指针
    uint64_t indirect_block;                    // 一级间接块指针
    uint8_t reserved[64];                       // 保留字段
    
    INode() : inode_number(0), file_type(FileType::REGULAR_FILE),
              permissions(0644), uid(0), gid(0), file_size(0),
              links_count(1), blocks_count(0), indirect_block(0) {
        time_t now = time(nullptr);
        create_time = modify_time = access_time = now;
        memset(direct_blocks, 0, sizeof(direct_blocks));
        memset(reserved, 0, sizeof(reserved));
    }
};

// ==================== 目录项结构 ====================
struct DirectoryEntry {
    uint32_t inode_number;                      // INode编号
    uint16_t rec_len;                           // 记录长度
    uint8_t name_len;                           // 文件名长度
    FileType file_type;                         // 文件类型
    char name[MAX_FILENAME + 1];                // 文件名
    
    DirectoryEntry() : inode_number(0), rec_len(sizeof(DirectoryEntry)),
                       name_len(0), file_type(FileType::REGULAR_FILE) {
        memset(name, 0, sizeof(name));
    }
    
    DirectoryEntry(uint32_t ino, const std::string& fname, FileType ftype)
        : inode_number(ino), rec_len(sizeof(DirectoryEntry)),
          name_len(fname.length()), file_type(ftype) {
        strncpy(name, fname.c_str(), MAX_FILENAME);
        name[MAX_FILENAME] = '\0';
    }
};

// ==================== LRU缓存节点 ====================
struct CacheNode {
    uint64_t block_number;                      // 块号
    std::vector<uint8_t> data;                  // 数据
    bool dirty;                                 // 脏标记
    time_t last_access;                         // 最后访问时间
    
    CacheNode(uint64_t bn) : block_number(bn), dirty(false) {
        data.resize(BLOCK_SIZE, 0);
        last_access = time(nullptr);
    }
};

// ==================== LRU块缓存 ====================
class LRUBlockCache {
private:
    size_t capacity_;                                           // 缓存容量
    std::list<std::shared_ptr<CacheNode>> cache_list_;         // LRU链表
    std::map<uint64_t, std::list<std::shared_ptr<CacheNode>>::iterator> cache_map_;  // 快速查找
    mutable std::mutex mutex_;                                  // 线程安全
    
    // 统计信息
    uint64_t hit_count_;
    uint64_t miss_count_;
    uint64_t eviction_count_;
    
public:
    explicit LRUBlockCache(size_t capacity = DEFAULT_CACHE_SIZE);
    ~LRUBlockCache();
    
    // 获取缓存块
    std::shared_ptr<CacheNode> get(uint64_t block_number);
    
    // 添加到缓存
    void put(uint64_t block_number, const std::vector<uint8_t>& data, bool dirty = false);
    
    // 标记为脏
    void mark_dirty(uint64_t block_number);
    
    // 刷新所有脏块
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> flush_dirty();
    
    // 清空缓存
    void clear();
    
    // 获取统计信息
    void get_statistics(uint64_t& hits, uint64_t& misses, uint64_t& evictions) const;
    
    // 获取命中率
    double get_hit_rate() const;
    
private:
    // 移动到最前面(最近使用)
    void move_to_front(std::list<std::shared_ptr<CacheNode>>::iterator it);
    
    // 淘汰最久未使用的块
    std::shared_ptr<CacheNode> evict_lru();
};

// ==================== 位图管理 ====================
class Bitmap {
private:
    std::vector<uint8_t> bitmap_;
    size_t size_;                                // 位图大小(位数)
    mutable std::mutex mutex_;
    
public:
    explicit Bitmap(size_t size);
    
    // 分配一个空闲位
    int64_t allocate();
    
    // 释放指定位
    bool free(size_t pos);
    
    // 检查位是否已分配
    bool is_allocated(size_t pos) const;
    
    // 获取空闲位数量
    size_t count_free() const;
    
    // 序列化/反序列化
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& data);
};

// ==================== 文件系统类 ====================
class FileSystem {
private:
    std::string disk_file_;                     // 磁盘文件路径
    std::fstream disk_;                         // 磁盘文件流
    SuperBlock super_block_;                    // 超级块
    std::unique_ptr<Bitmap> block_bitmap_;      // 块位图
    std::unique_ptr<Bitmap> inode_bitmap_;      // INode位图
    std::unique_ptr<LRUBlockCache> block_cache_; // LRU缓存
    std::map<uint32_t, INode> inode_cache_;     // INode缓存
    mutable std::mutex fs_mutex_;               // 文件系统锁
    
    bool initialized_;                          // 是否已初始化
    
public:
    explicit FileSystem(const std::string& disk_file, size_t cache_size = DEFAULT_CACHE_SIZE);
    ~FileSystem();
    
    // ===== 文件系统管理 =====
    bool format();                              // 格式化
    bool mount();                               // 挂载
    bool unmount();                             // 卸载
    void sync();                                // 同步到磁盘
    
    // ===== 文件/目录操作 =====
    int create_file(const std::string& path, uint32_t uid = 0);
    int create_directory(const std::string& path, uint32_t uid = 0);
    int remove(const std::string& path);
    int rename(const std::string& old_path, const std::string& new_path);
    
    // ===== 文件读写 =====
    ssize_t read_file(const std::string& path, std::vector<uint8_t>& buffer, 
                      size_t offset = 0, size_t length = 0);
    ssize_t write_file(const std::string& path, const std::vector<uint8_t>& data,
                       size_t offset = 0, bool append = false);
    
    // ===== 目录操作 =====
    std::vector<DirectoryEntry> list_directory(const std::string& path);
    bool exists(const std::string& path);
    
    // ===== 文件信息 =====
    bool get_inode(const std::string& path, INode& inode);
    bool stat(const std::string& path, INode& inode);
    
    // ===== 备份功能 =====
    bool create_backup(const std::string& backup_path);
    bool restore_backup(const std::string& backup_path);
    std::vector<std::string> list_backups(const std::string& backup_dir);
    
    // ===== 系统信息 =====
    SuperBlock get_super_block() const { return super_block_; }
    void get_cache_statistics(uint64_t& hits, uint64_t& misses, uint64_t& evictions);
    double get_cache_hit_rate();
    
    // ===== 调试功能 =====
    void dump_info() const;
    void check_integrity();
    
private:
    // ===== 底层操作 =====
    bool read_block(uint64_t block_number, std::vector<uint8_t>& data);
    bool write_block(uint64_t block_number, const std::vector<uint8_t>& data);
    
    // ===== INode操作 =====
    int allocate_inode();
    bool free_inode(uint32_t inode_number);
    bool read_inode(uint32_t inode_number, INode& inode);
    bool write_inode(uint32_t inode_number, const INode& inode);
    
    // ===== 块分配 =====
    int64_t allocate_block();
    bool free_block(uint64_t block_number);
    bool allocate_blocks_for_inode(INode& inode, size_t num_blocks);
    
    // ===== 路径解析 =====
    int parse_path(const std::string& path, uint32_t& parent_inode, std::string& filename);
    int find_inode_by_path(const std::string& path);
    
    // ===== 目录操作 =====
    bool add_directory_entry(uint32_t dir_inode, const DirectoryEntry& entry);
    bool remove_directory_entry(uint32_t dir_inode, const std::string& filename);
    std::vector<DirectoryEntry> read_directory_entries(uint32_t dir_inode);
    
    // ===== 超级块操作 =====
    bool read_super_block();
    bool write_super_block();
    
    // ===== 初始化 =====
    bool initialize_root_directory();
};

// ==================== 工具函数 ====================
namespace FSUtils {
    // 分割路径
    std::vector<std::string> split_path(const std::string& path);
    
    // 规范化路径
    std::string normalize_path(const std::string& path);
    
    // 连接路径
    std::string join_path(const std::string& dir, const std::string& file);
    
    // 获取父目录
    std::string get_parent_path(const std::string& path);
    
    // 获取文件名
    std::string get_filename(const std::string& path);
    
    // 时间转字符串
    std::string time_to_string(time_t t);
    
    // 文件类型转字符串
    std::string file_type_to_string(FileType type);
    
    // 权限转字符串
    std::string permissions_to_string(uint32_t perm);
}

#endif // FILESYSTEM_H
