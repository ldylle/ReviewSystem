#include "client.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // 默认参数
    std::string host = "127.0.0.1";
    int port = 8888;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --host HOST    Server host (default: 127.0.0.1)\n"
                      << "  --port PORT    Server port (default: 8888)\n"
                      << "  --help         Show this help message\n";
            return 0;
        }
    }
    
    std::cout << "=== Review System Client ===" << std::endl;
    std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
    
    // 创建客户端
    Client client(host, port);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // 运行CLI
    client.run_cli();
    
    std::cout << "Goodbye!" << std::endl;
    
    return 0;
}
