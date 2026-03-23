# ReviewSystem — 基于自定义文件系统的全栈科研审稿平台

<img width="2192" height="1187" alt="Login Page" src="https://github.com/user-attachments/assets/c4452bb1-1c8f-4d9e-81e1-29b90408762a" />

> 从 Raw Socket 到虚拟磁盘，不依赖任何数据库或 Web 框架的硬核系统级实践。

---

## 项目简介

ReviewSystem 是一个从零构建的全栈科研论文审稿系统。

区别于大多数应用直接调用现成数据库和 Web 框架，本项目选择在更底层的地方动手：用一个普通文件（`disk.img`）模拟了一套完整的类 Unix 虚拟文件系统（VFS），用原生 POSIX Socket 写了 HTTP 服务器和自定义 RPC 通信协议，把《操作系统》和《计算机网络》里学的东西真正跑起来了。

---

## 系统架构

<img width="2784" height="1536" alt="System Architecture" src="https://github.com/user-attachments/assets/e94ed14b-cb75-4178-a0cd-b09c2e616c33" />

整个系统分为四层：

- **Presentation Layer**：支持 Web 浏览器和 CLI 两种客户端接入
- **Network Interface Layer**：打造 HTTP 服务器 + 多线程 TCP Server，均基于原生 Socket
- **Business Logic Layer**：认证、论文管理、工作流、查重等核心逻辑
- **Data & File System Layer**：构建 VFS，所有数据持久化在 `disk.img` 中

---

## 核心特性

### 1. 手写虚拟文件系统

<img width="2816" height="1536" alt="VFS Disk Layout" src="https://github.com/user-attachments/assets/0ae2ad79-99e6-494e-9dfb-1fc765faa190" />

整个磁盘镜像（256MB）被划分为固定大小的 Block，布局如下：

| 区域 | Block 范围 | 说明 |
|------|-----------|------|
| SuperBlock | Block 0 | 磁盘元数据（总块数、Inode 数等） |
| Block Bitmap | Block 1–2 | 数据块使用状态，位级粒度 |
| Inode Bitmap | Block 3–4 | Inode 使用状态 |
| Inode Table | Block 5–516 | 共 8192 个 Inode |
| Data Blocks | Block 517– | 实际文件数据 |

每个 `struct INode`（256 bytes）通过直接指针和间接指针寻址数据块，支持多级路径解析（如 `/users/admin/papers`）。所有用户数据、论文文本、评分记录均存储在这套自研文件系统中。

### 2. 原生网络与 HTTP 服务器

没有使用任何第三方 Web 框架（Crow、Drogon 等），全部基于原生 Socket 编写：

- **TCP RPC 服务**：基于 `nlohmann/json` 序列化的自定义长连接协议，供 CLI 客户端使用
- **HTTP 服务器**：从零解析 HTTP 请求报文（Method、URI、Headers、Body），支持静态资源分发
- **RESTful API 路由**：`/api/*` 请求由 `web_api_controller` 分发到各业务 Manager，返回 JSON

### 3. 安全与审计

- **密码保护**：集成 OpenSSL，SHA-256 + 随机 Salt 存储密码哈希
- **Token 鉴权**：Session Token 机制，支持 AUTHOR / REVIEWER / ADMIN 三种角色
- **审计日志**：Write-Ahead 风格，记录所有关键操作（提交、打分、权限变更），确保评审过程可追溯

### 4. 学术查重与工作流

- **查重引擎**：TF-IDF 向量化 + 余弦相似度 + Jaccard 相似系数，自动标记疑似抄袭论文
- **状态机工作流**：论文状态按 `提交 → 审稿中 → 接受/拒绝` 流转，支持多轮审稿和审稿人调度

---

## 技术栈

| 类别 | 技术 |
|------|------|
| 核心语言 | C++17 |
| 构建系统 | CMake 3.10+ |
| 加密 | OpenSSL (Crypto) |
| 序列化 | nlohmann/json |
| 网络 | POSIX Sockets + std::thread / std::mutex |
| 前端 | 原生 HTML5 + JavaScript (Fetch API) |

---

## 快速开始

### 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libssl-dev nlohmann-json3-dev
```

### 2. 编译

```bash
git clone https://github.com/yourusername/ReviewSystem.git
cd ReviewSystem
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. 初始化虚拟磁盘

首次运行前需要格式化虚拟磁盘（初始化 SuperBlock 和 Inode 表）：

```bash
./fs_test format disk.img
```

终端显示 `Formatting virtual disk... Success` 即代表初始化完成。

### 4. 启动服务

**模式 A：Web GUI（推荐）**

```bash
./web_server --disk disk.img --port 8080
```

打开浏览器访问 `http://localhost:8080`

**模式 B：CLI 客户端**

```bash
# 终端 1：启动服务端
./server --disk disk.img --port 8888

# 终端 2：启动客户端
./client 127.0.0.1 8888
```

---

## 目录结构

```
ReviewSystem/
├── include/
│   ├── core/          # filesystem.h, protocol.h
│   ├── managers/      # auth_manager, review_manager
│   ├── network/       # web_server, network_server
│   └── utils/         # similarity_detector, audit_logger
├── src/
│   ├── filesystem.cpp # Inode 与数据块分配核心逻辑
│   ├── web_main.cpp   # HTTP 服务器主循环
│   └── ...
├── web/
│   ├── index.html     # Web GUI 单页应用
│   └── assets/
├── tests/
│   └── fs_test.cpp    # 文件系统黑盒/白盒测试
└── CMakeLists.txt
```

---

## License

MIT License — 欢迎 Fork 和学习交流。如果对你有帮助，欢迎点个 ⭐️