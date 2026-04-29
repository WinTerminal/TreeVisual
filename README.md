# TreeVisual - 跨平台智能目录树工具

一个现代化的命令行工具，用于生成美观的目录树结构。具备智能输出策略、多线程并行扫描、自动提权等特性。

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

## 安装

### 方式一：预编译二进制（推荐）

从 [Releases](https://github.com/WinTerminal/TreeVisual/releases) 下载对应平台的可执行文件，重命名为 `tree`（或 `tree.exe`），放入 `PATH` 目录。

### 方式二：源码编译

**依赖**：C++17 编译器、CMake 3.10+，Linux 需 `xclip` 或 `xsel`

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

## 使用方法

```bash
# 显示当前目录树
tree

# 显示指定目录
tree /path/to/directory

# 扫描用户主目录
tree ~
```

## 注意事项

本工具生成的可执行文件名为 `tree`，可能会覆盖系统原有的 `tree` 命令。如需保留原命令，请重命名本工具（例如 `treeviz`）或设置别名。

## 技术实现

- **语言**：C++17（std::filesystem, std::future, std::async）
- **并行阈值**：子目录数 ≥2 时开启异步任务
- **剪贴板**：Windows (Win32 API)，macOS (pbcopy)，Linux (xclip/xsel)
- **提权**：Windows (runas)，Unix (sudo)

## 贡献

欢迎 Issue 和 Pull Request。请确保代码风格一致并通过跨平台测试。

## 许可证

MIT License © 2026 WinTerminal