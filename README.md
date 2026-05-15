[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.3-7aa2f7?style=flat)]()
[![Release](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml)

# TreeVisual - Cross-platform Directory Tree Visualizer

A modern directory tree tool with TUI mode and **WebUI** (lazy-loading tree browser). Built in C++17, zero third-party dependencies.  
[Download latest release](https://github.com/WinTerminal/TreeVisual/releases/latest) | Auto-build on tag push via GitHub Actions.

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
- **i18n support** - English / Simplified Chinese / Traditional Chinese, auto-detects browser language
- **WebGL rendering (Beta)** - Force-directed graph visualization for directory trees
- **Settings panel** - Show hidden files, enable WebGL, etc.
- **Tokyo Night theme** - Responsive design, smooth animations

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
│   ├── tree.cpp          # Main source file (~1800 lines)
│   └── web/              # WebUI static assets
│       ├── index.html
│       ├── styles.css    # Tokyo Night theme
│       ├── app.js        # Core logic (tree render, lazy-load)
│       ├── i18n.js       # EN/zh-CN/zh-TW dictionary
│       └── webgl.js      # Force-directed graph renderer (Beta)
├── scripts/
│   └── embed-web.py      # Embeds web/ into tree.cpp at compile time
├── packaging/            # Release packaging templates
├── .github/workflows/
│   ├── build.yml         # CI build on push/PR
│   └── release.yml       # Auto release on tag push (3 platforms)
├── CMakeLists.txt
├── install.sh            # Linux/macOS installer (auto-download + compile)
└── install.bat           # Windows installer
```

## API

| Endpoint | Method | Params | Description |
|----------|--------|--------|-------------|
| `/api/tree` | GET | `path`, `show_hidden` | Single-level tree JSON |
| `/api/list` | GET | `path`, `show_hidden` | Flat directory listing JSON |
| `/api/settings` | GET/POST | - | Persist settings (showHidden, language, webgl) |
| `/api/home` | GET | - | Returns user home directory path |
| `/api/service/status` | GET | - | Daemon running status |

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| Filesystem | `std::filesystem` |
| Concurrency | `std::thread`, `std::atomic` |
| HTTP Server | Raw BSD sockets (zero deps) |
| Frontend | Vanilla HTML/CSS/JS (no frameworks) |
| WebGL | WebGL 1.0, GLSL shaders (Beta) |
| Clipboard | Win32 API / pbcopy / xclip |
| CI/CD | GitHub Actions (3 platforms) |

## Changelog

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
