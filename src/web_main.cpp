#include "web_api_controller.h"
#include <iostream>
#include <csignal>
#include <memory>

std::unique_ptr<ReviewWebApp> g_app;

void signal_handler(int signal) {
    std::cout << "\nShutting down web server..." << std::endl;
    if (g_app) {
        g_app->stop();
    }
    exit(0);
}

void print_banner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║   ██████╗ ███████╗██╗   ██╗██╗███████╗██╗    ██╗             ║
║   ██╔══██╗██╔════╝██║   ██║██║██╔════╝██║    ██║             ║
║   ██████╔╝█████╗  ██║   ██║██║█████╗  ██║ █╗ ██║             ║
║   ██╔══██╗██╔══╝  ╚██╗ ██╔╝██║██╔══╝  ██║███╗██║             ║
║   ██║  ██║███████╗ ╚████╔╝ ██║███████╗╚███╔███╔╝             ║
║   ╚═╝  ╚═╝╚══════╝  ╚═══╝  ╚═╝╚══════╝ ╚══╝╚══╝              ║
║                                                               ║
║           科研审稿系统 Web Server v1.0                        ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --port PORT          HTTP server port (default: 8080)\n"
              << "  --disk FILE          Disk image file (default: disk.img)\n"
              << "  --cache-size SIZE    Cache size in blocks (default: 256)\n"
              << "  --static DIR         Static files directory (default: ./web)\n"
              << "  --help               Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // 默认参数
    int port = 8080;
    std::string disk_file = "disk.img";
    size_t cache_size = 256;
    std::string static_dir = "./web";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--disk" && i + 1 < argc) {
            disk_file = argv[++i];
        } else if (arg == "--cache-size" && i + 1 < argc) {
            cache_size = std::stoi(argv[++i]);
        } else if (arg == "--static" && i + 1 < argc) {
            static_dir = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    print_banner();
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  HTTP Port:    " << port << std::endl;
    std::cout << "  Disk File:    " << disk_file << std::endl;
    std::cout << "  Cache Size:   " << cache_size << " blocks" << std::endl;
    std::cout << "  Static Dir:   " << static_dir << std::endl;
    std::cout << std::endl;
    
    // 创建应用
    g_app = std::make_unique<ReviewWebApp>(disk_file, port, cache_size);
    
    // 初始化
    std::cout << "Initializing system..." << std::endl;
    if (!g_app->initialize()) {
        std::cerr << "Failed to initialize. Try formatting the disk first:" << std::endl;
        std::cerr << "  ./fs_test format " << disk_file << std::endl;
        return 1;
    }
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  Server is running!" << std::endl;
    std::cout << "  Open your browser and visit: http://localhost:" << port << std::endl;
    std::cout << "  Default admin account: admin / admin123" << std::endl;
    std::cout << "  Press Ctrl+C to stop" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << std::endl;
    
    // 运行服务器
    g_app->run();
    
    return 0;
}
