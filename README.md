# TreeVisual – 跨平台智能目录树工具 (增强版 `tree` 命令)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)]()
[![Build Status](https://github.com/WinTerminal/TreeVisual/actions/workflows/build.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions)

**TreeVisual** 是一个现代化的命令行工具，用于生成漂亮的目录树结构，并根据文件/目录数量**自动决定输出方式**：少量时显示并复制到剪贴板，中等数量时仅复制，大量时自动保存为文件。支持**多线程并行扫描**（大型目录提速 2~5 倍），遇到权限不足时可**自动提权**，完美处理**符号链接循环**避免崩溃。

## ✨ 特性

- 🌲 **美观树形输出** – Unicode 连线，层次清晰。
- 📋 **智能输出策略**：
  - ≤50 行 → 终端显示 + 复制到剪贴板
  - 51–100 行 → 仅复制（不显示，避免刷屏）
  - >100 行 → 自动保存至 `~/TreeVisual/yy-mm-dd-hh-mm-ss-tree.txt`
- ⚡ **多线程加速** – 同一层级的子目录自动并行遍历。
- 🔐 **自动提权** – 遇到无法访问的目录时，询问是否以管理员/root 权限重新运行。
- 🪢 **安全处理符号链接** – 不会因循环链接或过深链接导致崩溃。
- 🌍 **跨平台** – Linux、macOS、Windows 运行一致。

## 📦 安装

### 方式一：预编译二进制（推荐）

从 [Releases](https://github.com/WinTerminal/TreeVisual/releases) 下载对应平台的可执行文件，重命名为 `tree`（或 `tree.exe`），放入 `PATH` 目录。

### 方式二：源码编译

**依赖**：C++17 编译器、CMake 3.10+，Linux 还需 `xclip` 或 `xsel`。

```bash
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual
mkdir build && cd build
cmake ..
cmake --build . --config Release
sudo cmake --install .   # 可选，安装到 /usr/local/bin
```

或直接使用 g++：

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree
```

Windows (MinGW)：

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree.exe -static -lshlwapi -lole32 -lshell32
```

🚀 使用方法

```bash
# 显示当前目录树
tree

# 显示指定目录
tree /path/to/directory

# 扫描用户主目录（多线程加速）
tree ~
```

⚠️ 与系统 tree 命令的兼容性

本工具生成的可执行文件名为 tree，可能会覆盖系统原有的 tree 命令。如需保留原命令，请重命名本工具（例如 treeviz）或设置别名。

🛠️ 技术实现

· 语言：C++17（std::filesystem, std::future, std::async）
· 并行阈值：子目录数 ≥2 时开启异步任务。
· 剪贴板：Windows (Win32 API)，macOS (pbcopy)，Linux (xclip/xsel)
· 提权：Windows (runas)，Unix (sudo)

🤝 贡献

欢迎 Issue 和 Pull Request。请确保代码风格一致并通过跨平台测试。

📄 许可证

MIT License © 2025 WinTerminal
