[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)

[English](./README.md) | [简体中文](./README-zh.md) | [繁體中文](./README-tc.md)

# TreeVisual - 跨平台智能目录树工具

一个现代化的命令行工具，用于生成美观的目录树结构。采用面向对象架构设计，具备智能输出策略、多线程并行扫描、自动提权等特性。

## 特性

- **美观树形输出** - 使用 Unicode 连线字符，层次清晰易读
- **智能输出策略**：
  - ≤50 行：终端显示 + 自动复制到剪贴板
  - 51-100 行：仅复制到剪贴板（避免刷屏）
  - >100 行：自动保存为文件
- **多线程加速** - 同一层级子目录并行遍历，大型目录提速 2-5 倍
- **自动提权** - 遇到权限不足时，自动询问是否以管理员/root 权限重新运行
- **安全处理符号链接** - 避免循环链接导致的崩溃
- **跨平台支持** - Linux、macOS、Windows 运行一致

## 架构设计

```
┌─────────────────────────────────────────────────┐
│                    App                         │
│               (主应用入口)                      │
└─────────────────────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           ▼                     ▼
    ┌──────────────┐    ┌─────────────────┐
    │  Directory  │    │ IPlatformHelper │
    │    Tree    │    │ (平台接口)      │
    └──────────────┘    └─────────────────┘
           │                    │
    ┌─────┴─────┐       ┌─────┴─────┐
    ▼           ▼       ▼           ▼
 ITraversal  Node   Windows   Unix
 Policy            Helper    Helper
```

### 核心类

| 类名 | 说明 |
|------|------|
| `PermissionErrorTracker` | 单例，线程安全地记录权限错误 |
| `Node` | 目录树节点，支持文件和目录类型 |
| `ITraversalPolicy` | 遍历策略接口，支持自定义实现 |
| `DefaultTraversalPolicy` | 默认遍历实现，过滤隐藏文件 |
| `DirectoryTree` | 串行目录树构建器 |
| `ParallelDirectoryTree` | 并行目录树构建器，继承自 `DirectoryTree` |
| `IOutputHandler` | 输出处理接口 |
| `ConsoleOutputHandler` | 控制台输出 |
| `ClipboardOutputHandler` | 剪贴板输出 |
| `FileOutputHandler` | 文件输出 |
| `IPlatformHelper` | 平台助手接口 |
| `WindowsPlatformHelper` | Windows 平台实现 |
| `UnixPlatformHelper` | Unix/Linux/macOS 平台实现 |
| `TreeFormatter` | 树格式化器，将 Node 转换为文本 |
| `App` | 主应用类，协调各组件 |

### 设计模式

- **单例模式**：`PermissionErrorTracker`
- **策略模式**：`ITraversalPolicy`、`IOutputHandler`、`IPlatformHelper`
- **模板方法模式**：`DirectoryTree::build()` → `buildNode()`
- **继承多态**：`ParallelDirectoryTree` 覆盖 `build()` 方法实现并行

## 安装

### 方式一：安装脚本（推荐）

项目已自带安装脚本，自动处理依赖安装、编译和 PATH 配置。

```bash
# Linux/macOS
./install.sh

# Windows (以管理员身份运行)
install.bat
```

安装脚本功能：
- Linux: 自动安装 `xclip` 剪贴板依赖
- 编译 TreeVisual
- 检测系统 `tree` 命令冲突，自动重命名为 `treeviz`
- 交互式引导添加到 PATH

### 方式二：预编译二进制

从 [Releases](https://github.com/WinTerminal/TreeVisual/releases) 下载对应平台的可执行文件。

### 方式三：CMake 编译

**依赖**：CMake 3.10+

```bash
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual
mkdir build && cd build
cmake ..
cmake --build . --config Release
sudo cmake --install .
```

### 方式四：直接编译

**依赖**：C++17 编译器（g++/clang++），Linux 需 `xclip` 或 `xsel`

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree
```

Windows (MinGW)：

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree.exe -static -lshlwapi -lole32 -lshell32
```

## 使用方法

```bash
# 显示当前目录树
tree

# 显示指定目录
tree /path/to/directory

# 扫描用户主目录
tree ~

# 显示隐藏文件
tree --hidden

# TUI 模式
tree -v

# 设置界面
tree --setting
```

## 技术栈

- **语言**：C++17
- **标准库**：`std::filesystem`、`std::thread`、`std::atomic`
- **剪贴板**：Windows (Win32 API)，macOS (pbcopy)，Linux (xclip/xsel)
- **提权**：Windows (runas)，Unix (sudo)

## 贡献

欢迎 Issue 和 Pull Request。请确保代码风格一致并通过跨平台测试。

## 许可证

MIT License © 2026 WinTerminal