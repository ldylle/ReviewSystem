#include "filesystem.h"
#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> <disk_file> [options]\n"
              << "Commands:\n"
              << "  format <disk_file>           - Format a new filesystem\n"
              << "  mount <disk_file>            - Mount and show info\n"
              << "  ls <disk_file> <path>        - List directory\n"
              << "  mkdir <disk_file> <path>     - Create directory\n"
              << "  write <disk_file> <path>     - Write test file\n"
              << "  read <disk_file> <path>      - Read file\n"
              << "  stats <disk_file>            - Show cache statistics\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    std::string disk_file = argv[2];
    
    if (command == "format") {
        std::cout << "Formatting filesystem: " << disk_file << std::endl;
        
        FileSystem fs(disk_file);
        if (!fs.format()) {
            std::cerr << "Format failed" << std::endl;
            return 1;
        }
        
        std::cout << "Format successful!" << std::endl;
        return 0;
    }
    
    // 其他命令需要挂载文件系统
    FileSystem fs(disk_file);
    
    if (!fs.mount()) {
        std::cerr << "Failed to mount filesystem" << std::endl;
        return 1;
    }
    
    if (command == "mount" || command == "info") {
        auto sb = fs.get_super_block();
        std::cout << "\n=== Filesystem Info ===\n"
                  << "Volume: " << sb.volume_name << "\n"
                  << "Total blocks: " << sb.total_blocks << "\n"
                  << "Free blocks: " << sb.free_blocks << "\n"
                  << "Total inodes: " << sb.total_inodes << "\n"
                  << "Free inodes: " << sb.free_inodes << "\n"
                  << "Block size: " << sb.block_size << " bytes\n"
                  << "Mount count: " << sb.mount_count << "\n"
                  << std::endl;
    } else if (command == "ls") {
        if (argc < 4) {
            std::cerr << "Usage: ls <disk_file> <path>" << std::endl;
            return 1;
        }
        
        std::string path = argv[3];
        auto entries = fs.list_directory(path);
        
        std::cout << "\nDirectory listing for " << path << ":\n";
        for (const auto& entry : entries) {
            std::cout << "  " << FSUtils::file_type_to_string(entry.file_type)
                      << "  " << entry.name << "\n";
        }
        std::cout << "\nTotal: " << entries.size() << " entries\n" << std::endl;
    } else if (command == "mkdir") {
        if (argc < 4) {
            std::cerr << "Usage: mkdir <disk_file> <path>" << std::endl;
            return 1;
        }
        
        std::string path = argv[3];
        if (fs.create_directory(path) >= 0) {
            std::cout << "Directory created: " << path << std::endl;
        } else {
            std::cerr << "Failed to create directory" << std::endl;
            return 1;
        }
    } else if (command == "write") {
        if (argc < 4) {
            std::cerr << "Usage: write <disk_file> <path>" << std::endl;
            return 1;
        }
        
        std::string path = argv[3];
        
        // 创建文件
        if (fs.create_file(path) < 0) {
            std::cerr << "Failed to create file" << std::endl;
            return 1;
        }
        
        // 写入测试数据
        std::string test_data = "Hello, Review System! This is a test file.\n";
        std::vector<uint8_t> data(test_data.begin(), test_data.end());
        
        if (fs.write_file(path, data) < 0) {
            std::cerr << "Failed to write file" << std::endl;
            return 1;
        }
        
        std::cout << "File written: " << path << " (" << data.size() << " bytes)" << std::endl;
    } else if (command == "read") {
        if (argc < 4) {
            std::cerr << "Usage: read <disk_file> <path>" << std::endl;
            return 1;
        }
        
        std::string path = argv[3];
        std::vector<uint8_t> data;
        
        if (fs.read_file(path, data) < 0) {
            std::cerr << "Failed to read file" << std::endl;
            return 1;
        }
        
        std::cout << "\nFile content (" << data.size() << " bytes):\n";
        std::cout << std::string(data.begin(), data.end()) << std::endl;
    } else if (command == "stats") {
        uint64_t hits, misses, evictions;
        fs.get_cache_statistics(hits, misses, evictions);
        double hit_rate = fs.get_cache_hit_rate();
        
        std::cout << "\n=== Cache Statistics ===\n"
                  << "Hits: " << hits << "\n"
                  << "Misses: " << misses << "\n"
                  << "Evictions: " << evictions << "\n"
                  << "Hit rate: " << (hit_rate * 100.0) << "%\n"
                  << std::endl;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    fs.unmount();
    return 0;
}
