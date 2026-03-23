#include "filesystem.h"
#include "auth_manager.h"
#include "review_manager.h"
#include "network_server.h"
#include <iostream>
#include <csignal>
#include <memory>

std::unique_ptr<NetworkServer> g_server;

void signal_handler(int signal) {
    std::cout << "\nShutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 默认参数
    int port = 8888;
    std::string disk_file = "disk.img";
    size_t cache_size = 256;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--disk" && i + 1 < argc) {
            disk_file = argv[++i];
        } else if (arg == "--cache-size" && i + 1 < argc) {
            cache_size = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port PORT          Server port (default: 8888)\n"
                      << "  --disk FILE          Disk image file (default: disk.img)\n"
                      << "  --cache-size SIZE    Cache size in blocks (default: 256)\n"
                      << "  --help               Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== Review System Server ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Disk: " << disk_file << std::endl;
    std::cout << "Cache: " << cache_size << " blocks" << std::endl;
    std::cout << std::endl;
    
    // 创建文件系统
    std::cout << "Loading filesystem..." << std::endl;
    auto fs = std::make_shared<FileSystem>(disk_file, cache_size);
    
    if (!fs->mount()) {
        std::cerr << "Failed to mount filesystem. Try formatting first with fs_test." << std::endl;
        return 1;
    }
    
    // 创建认证管理器
    std::cout << "Initializing authentication..." << std::endl;
    auto auth_manager = std::make_shared<AuthManager>(fs);
    if (!auth_manager->initialize()) {
        std::cerr << "Failed to initialize authentication" << std::endl;
        return 1;
    }
    
    // 创建审稿管理器
    std::cout << "Initializing review system..." << std::endl;
    auto review_manager = std::make_shared<ReviewManager>(fs, auth_manager);
    if (!review_manager->initialize()) {
        std::cerr << "Failed to initialize review system" << std::endl;
        return 1;
    }
    
    // 创建服务器
    std::cout << "Starting network server..." << std::endl;
    g_server = std::make_unique<NetworkServer>(port, fs, auth_manager, review_manager);
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (!g_server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;
    
    // 运行服务器
    g_server->run();
    
    return 0;
}
