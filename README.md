[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.4-7aa2f7?style=flat)]()
[![Release](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml)

<p align="center">
  <img src="TreeViz-logo.png" alt="TreeVisual Logo" width="128" height="128">
</p>

# TreeVisual - Cross-platform Directory Tree Visualizer

A modern directory tree tool with TUI mode and **WebUI** (lazy-loading tree browser with Canvas hardware-accelerated rendering). Built in C++17, zero third-party dependencies.  
[Download latest release](https://github.com/WinTerminal/TreeVisual/releases/latest) | Auto-build on tag push via GitHub Actions.  
[Live Web Preview](https://winterminal.github.io/TreeVisual/) (static UI)

## Features

### CLI / TUI Mode
- **Beautiful tree output** - Unicode branch characters, clear hierarchy
- **Intelligent output strategies**: terminal display / clipboard / file auto-switching
- **Multi-threaded acceleration** - Parallel traversal for large directories (2-5x speedup)
- **Auto privilege elevation** - Prompt to re-run as admin/root on permission denied
- **Safe symlink handling** - Prevents crashes from circular links
- **Daemon service mode** (`--service on|off|status`) - Background WebUI server

### WebUI Mode (`--web`)
- **Lazy-loading tree structure** - Only loads one level at a time; expand directories on demand
- **Canvas 2D HW-accelerated rendering** - Character-based tree with smooth expand/collapse animation, Unicode box drawing
- **4 themes × 2 modes** (Mocha, Macchiato, Frappé, Latte × Dark/Light) — 8 color schemes
- **Autocomplete** - Debounced, case-insensitive prefix path suggestions with directories-first sorting
- **i18n support** - English / Simplified Chinese, auto-detects browser language
- **Settings panel** - Font size, font family, theme, animation speed, hidden files, service toggle
- **Expand/collapse animation** — Staggered fade-in/slide for both open and close
- **Custom colors** - Pick your own background, text, directory, and root colors
- **GitHub Pages preview** — Static UI available at [winterminal.github.io/TreeVisual](https://winterminal.github.io/TreeVisual/)

## Quick Start

```bash
# Option 1: Download pre-built package
# https://github.com/WinTerminal/TreeVisual/releases/latest

# Option 2: Build from source (auto-downloads dependencies)
./install.sh    # Linux/macOS
install.bat     # Windows

# Option 3: Manual build
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual && mkdir build && cd build
cmake .. && make -j$(nproc)
./tree --web
```

The installer automatically downloads source code and web assets, embeds them into the binary, compiles, and cleans up.  
It detects your system locale and encoding (UTF-8/GBK/Big5) to display messages in English, 简体中文 or 繁體中文.

## Usage

```
tree [path]              Show directory tree (CLI)
tree -v                  Interactive TUI mode
tree --web               Start WebUI server (port 7200)
tree --web stop          Stop running WebUI server
tree --service on        Start as background daemon
tree --service off       Stop daemon service
tree --service status    Check daemon status
tree --hidden            Show hidden files/directories
tree --setting           Open settings panel
```

## Architecture

```
TreeVisual/
├── src/
│   ├── tree.cpp          # Main source file (~1860 lines)
│   └── web/              # WebUI static assets
│       ├── index.html
│       ├── styles.css    # 4 Catppuccin-based themes, dark/light modes
│       ├── app.js        # Core logic (tree render, lazy-load, settings, service)
│       ├── i18n.js       # EN/zh dictionary
│       └── webgl.js      # Canvas 2D HW-accelerated character-tree renderer
├── Preview/              # GitHub Pages deployment copy
├── scripts/
│   └── embed-web.py      # Embeds web/ into tree.cpp at compile time
├── packaging/            # Release packaging templates
├── .github/workflows/
│   ├── build.yml         # CI build on push/PR
│   ├── release.yml       # Auto release on tag push (3 platforms)
│   └── pages.yml         # Deploy web UI to GitHub Pages
├── CMakeLists.txt
├── install.sh            # Linux/macOS installer (auto-download + compile)
└── install.bat           # Windows installer
```

## API

| Endpoint | Method | Params | Description |
|----------|--------|--------|-------------|
| `/api/tree` | GET | `path`, `show_hidden` | Single-level tree JSON |
| `/api/list` | GET | `path`, `show_hidden` | Flat directory listing JSON |
| `/api/settings` | GET/POST | - | Persist settings (theme, mode, fontSize, etc.) |
| `/api/home` | GET | - | Returns user home directory path |
| `/api/service/status` | GET | - | Daemon running status |
| `/api/service/start` | POST | - | Start daemon in background |
| `/api/service/stop` | POST | - | Stop running daemon |

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| Filesystem | `std::filesystem` |
| Concurrency | `std::thread`, `std::atomic` |
| HTTP Server | Raw BSD sockets (zero deps) |
| Frontend | Vanilla HTML/CSS/JS (no frameworks) |
| Canvas | Canvas 2D `getContext('2d')` — HW-accelerated character tree |
| Clipboard | Win32 API / pbcopy / xclip |
| CI/CD | GitHub Actions (3 platforms) |

## Changelog

### v1.1.4
- **Collapse animation** — Expand and collapse both have staggered slide + fade animation
- **Animation optimization** — Easing changed from easeOutQuad to easeOutCubic; 收起动画全新实现
- **Service mode fix** — `isRunning()` 正确检测僵尸进程；`stop()` 支持自杀保护；端口共享 `SO_REUSEPORT`
- **Frontend service toggle fix** — 点击 Stop 后界面及时更新为 Off（catch 处理器处理连接断开）
- **GitHub Pages preview** — `Preview/` 目录 + `pages.yml` workflow，静态 UI 在线预览
- **Version auto-sync** — Release workflow 自动更新 `index.html` 和 `README.md` 版本号并推送

### v1.1.3
- **Auto Release workflow**: Tag push triggers cross-platform build, packaging, and GitHub Release
- **Install script enhancement**: Auto-downloads web assets, embeds into binary, cleans up after compile
- **i18n: 3 languages**: English / 简体中文 / 繁體中文, auto-detects locale encoding (UTF-8/GBK/Big5)
- **PATH prompt**: Default `(Y/n)` — press Enter to add, `n` to skip
- **WebUI embedded fallback**: `WEB_EMBEDDED` compile flag bakes web files into the binary

### v1.1.2
- **Bugfix: 14 issues fixed**: Windows Service crash (inverted condition), hardcoded home path, CSS orphans, MSVC string limit, CSP header, etc.
- **Cross-platform packages**: Pre-built binaries for Linux/macOS/Windows via GitHub Actions

### v1.1.1
- Service mode (`--service on|off|status`)
- Settings persistence (`/api/settings`)
- WebUI start/stop subcommands (`--web start|stop`)

### v1.1.0
- WebUI lazy loading, frontend separation into `src/web/`
- i18n support (Chinese/English)
- WebGL Beta, CSS overhaul, settings panel
- Static file serving with fallback

## Contributing

Issues and PRs welcome. Please ensure cross-platform compatibility.

## License

MIT © 2026 WinTerminal
