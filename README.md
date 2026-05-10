[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.0-7aa2f7?style=flat)]()

# TreeVisual - Cross-platform Directory Tree Visualizer

A modern directory tree tool with TUI mode and **WebUI** (lazy-loading tree browser). Built in C++17, zero third-party dependencies.

## Features

### CLI / TUI Mode
- **Beautiful tree output** - Unicode branch characters, clear hierarchy
- **Intelligent output strategies**: terminal display / clipboard / file auto-switching
- **Multi-threaded acceleration** - Parallel traversal for large directories (2-5x speedup)
- **Auto privilege elevation** - Prompt to re-run as admin/root on permission denied
- **Safe symlink handling** - Prevents crashes from circular links

### WebUI Mode (`--web`) - **NEW in v1.1.0**
- **Lazy-loading tree structure** - Only loads one level at a time; expand directories on demand
- **Separated frontend files** - HTML / CSS / JS extracted into `src/web/` for easy customization
- **Multi-language support** - Chinese / English toggle, auto-detects browser language
- **WebGL rendering (Beta)** - Force-directed graph visualization for directory trees
- **Settings panel** - Show hidden files, enable WebGL, etc.
- **Optimized CSS** - Tokyo Night theme, responsive design, smooth animations

## Architecture

```
TreeVisual/
├── src/
│   ├── tree.cpp          # Main source file (~1500 lines)
│   └── web/              # WebUI static assets
│       ├── index.html    # Page skeleton + i18n attributes
│       ├── styles.css    # Tokyo Night theme + responsive
│       ├── app.js        # Core logic (tree render, lazy-load)
│       ├── i18n.js       # Chinese/English dictionary
│       └── webgl.js      # Force-directed graph renderer (Beta)
├── CMakeLists.txt
├── install.sh            # Unix installer
└── install.bat           # Windows installer
```

| Component | Description |
|-----------|-------------|
| `WebServer` | Embedded HTTP server (raw sockets), static file serving |
| `dirTreeToJson()` | Single-level API: returns children + `hasChildren` flag |
| `getWebRoot()` | Multi-candidate path resolution with fallback |
| `i18n.js` | Frontend-only dictionary, no backend changes needed |

## Quick Start

```bash
# Clone & build
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run CLI mode
./build/tree /path/to/dir

# Run WebUI (opens http://127.0.0.1:7200/)
./build/tree --web
```

Or use the installer:
```bash
./install.sh    # Linux/macOS
install.bat     # Windows
```

## Usage

```
tree [path]              Show directory tree (CLI)
tree -v                  Interactive TUI mode
tree --web               Start WebUI server (port 7200)
tree --hidden            Show hidden files/directories
tree --setting           Open settings panel
```

## WebUI API

| Endpoint | Method | Params | Description |
|----------|--------|--------|-------------|
| `/api/tree` | GET | `path`, `show_hidden` | Single-level tree JSON |
| `/api/list` | GET | `path`, `show_hidden` | Flat directory listing JSON |

Static files served from `src/web/`: `index.html`, `styles.css`, `app.js`, `i18n.js`, `webgl.js`

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

## Changelog

### v1.1.0
- **WebUI lazy loading**: Single-layer API, async child expansion
- **Frontend separation**: Extracted HTML/CSS/JS into `src/web/`
- **i18n support**: Chinese/English language switching
- **WebGL Beta**: Force-directed graph visualization
- **CSS overhaul**: CSS variables, responsive layout, animations
- **Settings panel**: Show hidden files, WebGL toggle
- **Static file serving**: Backend serves web assets from disk with fallback

### v1.0.2
- Initial WebUI release (embedded HTML)

### v1.0.1
- First public release (CLI/TUI only)

## Contributing

Issues and PRs welcome. Please ensure cross-platform compatibility.

## License

MIT © 2026 WinTerminal
