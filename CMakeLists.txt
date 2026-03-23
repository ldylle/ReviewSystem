cmake_minimum_required(VERSION 3.10)
project(ReviewSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==================== 编译选项 ====================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Debug 模式
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -DDEBUG)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# ==================== 功能开关 ====================
option(BUILD_WEB_SERVER "Build Web GUI server" ON)
option(BUILD_CLI_CLIENT "Build command line client" ON)
option(BUILD_ENHANCED_FEATURES "Build enhanced features (workflow, similarity, audit)" ON)
option(BUILD_TESTS "Build test executables" ON)

# ==================== 查找依赖库 ====================
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 查找 nlohmann_json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package, using system headers")
    # 检查是否能找到头文件
    find_path(NLOHMANN_JSON_INCLUDE nlohmann/json.hpp)
    if(NOT NLOHMANN_JSON_INCLUDE)
        message(WARNING "nlohmann/json.hpp not found! Install with: sudo apt-get install nlohmann-json3-dev")
    endif()
else()
    message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")
endif()

# ==================== 包含目录 ====================
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==================== 核心库定义 ====================

# 1. 文件系统库 - 核心模块
add_library(filesystem STATIC 
    filesystem.cpp
)
target_link_libraries(filesystem PUBLIC Threads::Threads)

# 2. 协议库 - 网络通信协议
add_library(protocol STATIC 
    protocol.cpp
)
if(nlohmann_json_FOUND)
    target_link_libraries(protocol PUBLIC nlohmann_json::nlohmann_json)
endif()

# 3. 认证管理库 - 用户认证和会话管理
add_library(auth_manager STATIC 
    auth_manager.cpp
)
target_link_libraries(auth_manager PUBLIC 
    filesystem 
    OpenSSL::Crypto
)

# 4. 审稿管理库 - 论文和评审管理
add_library(review_manager STATIC 
    review_manager.cpp
)
target_link_libraries(review_manager PUBLIC 
    filesystem 
    auth_manager
)

# ==================== 增强功能库 ====================
if(BUILD_ENHANCED_FEATURES)
    message(STATUS "Building enhanced features...")
    
    # 5. 审计日志库 - 操作审计和WAL日志
    add_library(audit_logger STATIC 
        audit_logger.cpp
    )
    target_link_libraries(audit_logger PUBLIC Threads::Threads)
    
    # 6. 审稿流程库 - 多轮审稿、截止日期、通知
    add_library(review_workflow STATIC 
        review_workflow.cpp
    )
    target_link_libraries(review_workflow PUBLIC 
        review_manager 
        Threads::Threads
    )
    
    # 7. 相似度检测库 - 查重和文本分析
    add_library(similarity_detector STATIC 
        similarity_detector.cpp
    )
    target_link_libraries(similarity_detector PUBLIC Threads::Threads)
    
    # 定义宏以便代码中检测增强功能
    add_compile_definitions(ENHANCED_FEATURES_ENABLED)
endif()

# 8. 网络服务器库 - TCP Socket 服务器
add_library(network_server STATIC 
    network_server.cpp
)
target_link_libraries(network_server PUBLIC 
    filesystem 
    protocol 
    auth_manager 
    review_manager 
    Threads::Threads
)

# 链接增强功能库
if(BUILD_ENHANCED_FEATURES)
    target_link_libraries(network_server PUBLIC
        audit_logger
        review_workflow
        similarity_detector
    )
endif()

# ==================== 可执行文件 ====================

# CLI 服务器 (TCP Socket)
add_executable(server server_main.cpp)
target_link_libraries(server network_server)

# CLI 客户端
if(BUILD_CLI_CLIENT)
    add_executable(client 
        client_main.cpp 
        client.cpp
    )
    target_link_libraries(client protocol Threads::Threads)
endif()

# Web GUI 服务器 (HTTP)
if(BUILD_WEB_SERVER)
    add_executable(web_server web_main.cpp)
    target_link_libraries(web_server 
        filesystem 
        auth_manager 
        review_manager 
        Threads::Threads
    )
    if(BUILD_ENHANCED_FEATURES)
        target_link_libraries(web_server
            audit_logger
            review_workflow
            similarity_detector
        )
    endif()
    
    # 复制 Web 静态文件到构建目录
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/web)
    if(EXISTS ${CMAKE_SOURCE_DIR}/web/index.html)
        configure_file(${CMAKE_SOURCE_DIR}/web/index.html ${CMAKE_BINARY_DIR}/web/index.html COPYONLY)
    endif()
endif()

# ==================== 测试工具 ====================
if(BUILD_TESTS)
    # 文件系统测试工具
    add_executable(fs_test fs_test.cpp)
    target_link_libraries(fs_test filesystem)
    
    # 可以添加更多测试
    # add_executable(auth_test tests/auth_test.cpp)
    # target_link_libraries(auth_test auth_manager)
    
    # add_executable(protocol_test tests/protocol_test.cpp)
    # target_link_libraries(protocol_test protocol)
endif()

# ==================== 安装规则 ====================
install(TARGETS server RUNTIME DESTINATION bin)
if(BUILD_CLI_CLIENT)
    install(TARGETS client RUNTIME DESTINATION bin)
endif()
if(BUILD_WEB_SERVER)
    install(TARGETS web_server RUNTIME DESTINATION bin)
    install(DIRECTORY web/ DESTINATION share/reviewsystem/web)
endif()

# ==================== 自定义目标 ====================

# 格式化磁盘
add_custom_target(format
    COMMAND ${CMAKE_BINARY_DIR}/fs_test format disk.img
    DEPENDS fs_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Formatting virtual disk..."
)

# 运行服务器
add_custom_target(run-server
    COMMAND ${CMAKE_BINARY_DIR}/server --disk disk.img --port 8888
    DEPENDS server format
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Starting TCP server..."
)

# 运行Web服务器
if(BUILD_WEB_SERVER)
    add_custom_target(run-web
        COMMAND ${CMAKE_BINARY_DIR}/web_server --disk disk.img --port 8080
        DEPENDS web_server format
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Starting Web server..."
    )
endif()

# ==================== 打印配置摘要 ====================
message(STATUS "")
message(STATUS "╔═══════════════════════════════════════════════════════════╗")
message(STATUS "║         ReviewSystem Build Configuration                  ║")
message(STATUS "╠═══════════════════════════════════════════════════════════╣")
message(STATUS "║  Version:              ${PROJECT_VERSION}                            ║")
message(STATUS "║  C++ Standard:         C++${CMAKE_CXX_STANDARD}                             ║")
message(STATUS "║  Build Type:           ${CMAKE_BUILD_TYPE}                          ║")
message(STATUS "║  Enhanced Features:    ${BUILD_ENHANCED_FEATURES}                            ║")
message(STATUS "║  Web Server:           ${BUILD_WEB_SERVER}                            ║")
message(STATUS "║  CLI Client:           ${BUILD_CLI_CLIENT}                            ║")
message(STATUS "║  Tests:                ${BUILD_TESTS}                            ║")
message(STATUS "╚═══════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "Build targets:")
message(STATUS "  - server      : TCP Socket server (CLI)")
if(BUILD_CLI_CLIENT)
    message(STATUS "  - client      : Command line client")
endif()
if(BUILD_WEB_SERVER)
    message(STATUS "  - web_server  : HTTP Web GUI server")
endif()
if(BUILD_TESTS)
    message(STATUS "  - fs_test     : Filesystem test tool")
endif()
message(STATUS "")
message(STATUS "Quick start:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make -j$(nproc)")
message(STATUS "  ./fs_test format disk.img")
if(BUILD_WEB_SERVER)
    message(STATUS "  ./web_server --disk disk.img")
    message(STATUS "  # Open http://localhost:8080 in browser")
endif()
message(STATUS "")