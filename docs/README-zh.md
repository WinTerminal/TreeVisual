[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.4-7aa2f7?style=flat)]()
[![Release](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml)

**[English](../README.md) | [简体中文](README-zh.md) | [繁體中文](README-tc.md)**

<p align="center">
  <img src="../TreeViz-logo.png" alt="TreeVisual Logo" width="128" height="128">&nbsp;&nbsp;<img src="../TreeViz-logo-light.png" alt="TreeVisual Logo" width="128" height="128">
  <br>
  <sub>暗色模式 &nbsp;|&nbsp; 亮色模式</sub>
</p>

# TreeVisual - 跨平台目录树可视化工具

现代化的目录树工具，支持 TUI 模式和 **WebUI**（懒加载树浏览器 + Canvas 硬件加速渲染）。基于 C++17 构建，零第三方依赖。
[下载最新版本](https://github.com/WinTerminal/TreeVisual/releases/latest) | 通过 GitHub Actions 自动构建。  
[在线预览](https://winterminal.github.io/TreeVisual/)（静态界面）

## 功能特性

### CLI / TUI 模式
- **美观的树形输出** - Unicode 连线字符，层次清晰
- **智能输出策略** - 终端显示 / 剪贴板 / 文件自动切换
- **多线程加速** - 并行遍历大型目录（2-5 倍提速）
- **自动提权** - 权限不足时提示以管理员/root 身份重新运行
- **安全处理符号链接** - 防止循环链接导致崩溃
- **守护进程服务模式** (`--service on|off|status`) - 后台 WebUI 服务

### WebUI 模式 (`--web`)
- **懒加载树结构** - 按需展开目录，仅加载当前层级
- **Canvas 2D 硬件加速渲染** - 字符树形展示，平滑展开/收起动画，Unicode 方框绘制
- **4 种主题 × 2 种模式**（Mocha、Macchiato、Frappé、Latte × 暗色/亮色）— 共 8 套配色方案
- **路径自动补全** - 防抖、大小写不敏感前缀匹配，目录优先排序
- **国际化支持** - 英文 / 简体中文，自动检测浏览器语言
- **设置面板** - 字体大小、字体族、主题、动画速度、隐藏文件、服务开关
- **展开/收起动画** - 渐入+滑动交错动画（打开和关闭均支持）
- **自定义颜色** - 自选背景色、文字色、目录色、根目录色
- **JS 模式（Beta）** - 无需后端，使用浏览器 File System API 访问本地文件系统
- **GitHub Pages 预览** — 静态界面可访问 [winterminal.github.io/TreeVisual](https://winterminal.github.io/TreeVisual/)

## 快速开始

```bash
# 方式一：下载预编译包
# https://github.com/WinTerminal/TreeVisual/releases/latest

# 方式二：从源码构建（自动下载依赖）
./install.sh    # Linux/macOS
install.bat     # Windows

# 方式三：手动编译
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual && mkdir build && cd build
cmake .. && make -j$(nproc)
./tree --web
```

安装脚本会自动下载源码和 Web 资源，将其嵌入二进制文件，编译并清理。  
脚本会检测系统区域设置和编码（UTF-8/GBK/Big5），以英文、简体中文或繁體中文显示消息。

## 使用方法

```
tree [路径]              显示目录树（CLI）
tree -v                  交互式 TUI 模式
tree --web               启动 WebUI 服务器（端口 7200）
tree --web stop          停止运行中的 WebUI 服务器
tree --service on        启动后台守护进程
tree --service off       停止守护进程
tree --service status    检查守护进程状态
tree --hidden            显示隐藏文件/目录
tree --setting           打开设置面板
```

## 架构

```
TreeVisual/
├── src/
│   ├── tree.cpp          # 主源码文件（约 1860 行）
│   └── web/              # WebUI 静态资源
│       ├── index.html
│       ├── styles.css    # 4 种 Catppuccin 主题，暗色/亮色模式
│       ├── app.js        # 核心逻辑（树形渲染、懒加载、设置、服务）
│       ├── i18n.js       # 英/中词典
│       └── webgl.js      # Canvas 2D 硬件加速字符树渲染器
├── Preview/              # GitHub Pages 部署副本
├── scripts/
│   └── embed-web.py      # 编译时将 web/ 嵌入 tree.cpp
├── packaging/            # 发布打包模板
├── .github/workflows/
│   ├── build.yml         # 推送/PR 时 CI 构建
│   ├── release.yml       # 标签推送时自动发布（3 平台）
│   └── pages.yml         # 部署 Web UI 到 GitHub Pages
├── CMakeLists.txt
├── install.sh            # Linux/macOS 安装脚本（自动下载+编译）
└── install.bat           # Windows 安装脚本
```

## API

| 端点 | 方法 | 参数 | 说明 |
|------|------|------|------|
| `/api/tree` | GET | `path`, `show_hidden` | 单层树 JSON |
| `/api/list` | GET | `path`, `show_hidden` | 扁平目录列表 JSON |
| `/api/settings` | GET/POST | - | 持久化设置（主题、模式、字体等） |
| `/api/home` | GET | - | 返回用户主目录路径 |
| `/api/service/status` | GET | - | 守护进程运行状态 |
| `/api/service/start` | POST | - | 后台启动守护进程 |
| `/api/service/stop` | POST | - | 停止运行中的守护进程 |

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| 文件系统 | `std::filesystem` |
| 并发 | `std::thread`, `std::atomic` |
| HTTP 服务器 | 原生 BSD socket（零依赖） |
| 前端 | 原生 HTML/CSS/JS（无框架） |
| Canvas | Canvas 2D `getContext('2d')` — 硬件加速字符树 |
| 剪贴板 | Win32 API / pbcopy / xclip |
| CI/CD | GitHub Actions（3 平台） |

## 更新日志

### v1.1.4
- **收起动画** — 展开和收起均支持交错滑动+渐变动画
- **动画优化** — 缓动函数从 easeOutQuad 改为 easeOutCubic；全新实现收起动画
- **服务模式修复** — `isRunning()` 正确检测僵尸进程；`stop()` 支持自杀保护；端口共享 `SO_REUSEPORT`
- **前端服务开关修复** — 点击 Stop 后界面及时更新为 Off（catch 处理器处理连接断开）
- **GitHub Pages 预览** — `Preview/` 目录 + `pages.yml` 工作流，静态界面在线预览
- **版本号自动同步** — Release 工作流自动更新 `index.html` 和 `README.md` 版本号并推送

### v1.1.3
- **自动发布工作流**：标签推送触发跨平台构建、打包和 GitHub Release
- **安装脚本增强**：自动下载 Web 资源，嵌入二进制文件，编译后清理
- **国际化：3 种语言**：英文 / 简体中文 / 繁體中文，自动检测区域编码（UTF-8/GBK/Big5）
- **PATH 提示**：默认 `(Y/n)` — 回车确认添加，`n` 跳过
- **WebUI 内嵌回退**：`WEB_EMBEDDED` 编译标志将 Web 文件烘焙进二进制

### v1.1.2
- **Bug 修复：14 个问题修复**：Windows Service 崩溃（条件反转）、硬编码主目录路径、CSS 孤儿规则、MSVC 字符串限制、CSP 头等
- **跨平台包**：通过 GitHub Actions 提供 Linux/macOS/Windows 预编译二进制

### v1.1.1
- 服务模式 (`--service on|off|status`)
- 设置持久化 (`/api/settings`)
- WebUI 启停子命令 (`--web start|stop`)

### v1.1.0
- WebUI 懒加载，前端分离至 `src/web/`
- 国际化支持（中/英）
- WebGL 测试版，CSS 重构，设置面板
- 静态文件服务及回退机制

## 贡献

欢迎提交 Issue 和 Pull Request。请确保跨平台兼容性。

## 许可证

MIT © 2026 WinTerminal
