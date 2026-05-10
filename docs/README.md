[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.0-7aa2f7?style=flat)]()

[English](./README.md) | [简体中文](./README-zh.md) | [繁體中文](./README-tc.md)

# TreeVisual - Cross-platform Directory Tree Visualizer

A modern directory tree tool with **TUI mode** and **WebUI** (lazy-loading tree browser). Built in C++17, zero third-party dependencies.

## Features

### CLI / TUI Mode
- **Beautiful tree output** - Unicode branch characters, clear hierarchy
- **Intelligent output strategies**:
  - ≤50 lines: display in terminal + copy to clipboard
  - 51-100 lines: copy to clipboard only (prevent flooding)
  - >100 lines: auto-save to file
- **Multi-threaded acceleration** - Parallel traversal of same-level subdirectories, 2-5x speedup for large directories
- **Auto privilege elevation** - When permission denied, prompt to re-run as admin/root
- **Safe symlink handling** - Prevent crashes from circular links

### WebUI Mode (`--web`) — *New in v1.1.0*
- **Lazy-loading tree** - Single-layer API, expand directories on demand (no more lag on large dirs!)
- **Separated frontend files** - `src/web/index.html`, `styles.css`, `app.js` for easy customization
- **Multi-language support** - Chinese / English toggle, auto-detects browser language
- **WebGL rendering (Beta)** - Force-directed graph visualization with drag/zoom
- **Settings panel** - Show hidden files, enable WebGL rendering
- **Optimized CSS** - Tokyo Night theme, responsive design, smooth transitions

## Architecture Design

```
┌─────────────────────────────────────────────────┐
│                    App                         │
│               (Main entry)                     │
└─────────────────────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           ▼                     ▼
    ┌──────────────┐    ┌─────────────────┐
    │  Directory  │    │ IPlatformHelper │
    │    Tree    │    │ (Platform API) │
    └──────────────┘    └─────────────────┘
           │                    │
    ┌─────┴─────┐       ┌─────┴─────┐
    ▼           ▼       ▼           ▼
 ITraversal  Node   Windows   Unix
 Policy            Helper    Helper
```

### Core Classes

| Class | Description |
|-------|------------|
| `PermissionErrorTracker` | Singleton, thread-safe permission error recording |
| `Node` | Directory tree node, supports file and directory types |
| `ITraversalPolicy` | Traversal policy interface, supports custom implementations |
| `DefaultTraversalPolicy` | Default traversal, filters hidden files |
| `DirectoryTree` | Serial directory tree builder |
| `ParallelDirectoryTree` | Parallel directory tree builder, inherits `DirectoryTree` |
| `IOutputHandler` | Output handler interface |
| `ConsoleOutputHandler` | Console output |
| `ClipboardOutputHandler` | Clipboard output |
| `FileOutputHandler` | File output |
| `IPlatformHelper` | Platform helper interface |
| `WindowsPlatformHelper` | Windows platform implementation |
| `UnixPlatformHelper` | Unix/Linux/macOS platform implementation |
| `TreeFormatter` | Tree formatter, converts Node to text |
| `App` | Main application class, coordinates components |

### Design Patterns

- **Singleton**: `PermissionErrorTracker`
- **Strategy**: `ITraversalPolicy`, `IOutputHandler`, `IPlatformHelper`
- **Template Method**: `DirectoryTree::build()` → `buildNode()`
- **Inheritance Polymorphism**: `ParallelDirectoryTree` overrides `build()` for parallel execution

## Installation

### Method 1: Installation Script (Recommended)

The project includes installation scripts that automatically handle dependency installation, compilation, and PATH configuration.

```bash
# Linux/macOS
./install.sh

# Windows (run as administrator)
install.bat
```

Installation script features:
- Linux: auto-install `xclip` clipboard dependency
- Compile TreeVisual
- Detect system `tree` command conflict, auto-rename to `treeviz`
- Interactive PATH configuration guide

### Method 2: Pre-built Binaries

Download pre-built binaries from [Releases](https://github.com/WinTerminal/TreeVisual/releases).

### Method 3: CMake Build

**Requirements**: CMake 3.10+

```bash
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual
mkdir build && cd build
cmake ..
cmake --build . --config Release
sudo cmake --install .
```

### Method 4: Direct Compilation

**Requirements**: C++17 compiler (g++/clang++), Linux requires `xclip` or `xsel`

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree
```

Windows (MinGW):

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree.exe -static -lshlwapi -lole32 -lshell32
```

## Usage

```bash
# Show current directory tree
tree

# Show specified directory
tree /path/to/directory

# Show hidden files
tree --hidden

# TUI mode (interactive)
tree -v

# Settings
tree --setting

# WebUI mode (opens http://127.0.0.1:7200/)
tree --web
```

### WebUI API Endpoints

| Endpoint | Params | Description |
|----------|--------|-------------|
| `GET /api/tree` | `path`, `show_hidden` | Single-level tree JSON with lazy-loading support |
| `GET /api/list` | `path`, `show_hidden` | Flat directory listing |

### WebUI Frontend Files (`src/web/`)

| File | Description |
|------|-------------|
| `index.html` | Page skeleton, i18n attributes, WebGL canvas |
| `styles.css` | Tokyo Night theme, responsive layout, animations |
| `app.js` | Tree rendering, lazy-load navigation, settings |
| `i18n.js` | Chinese / English dictionary, language switching |
| `webgl.js` | Force-directed graph renderer (**Beta**) |

## Tech Stack

- **Language**: C++17
- **Standard Library**: `std::filesystem`, `std::thread`, `std::atomic`
- **Clipboard**: Windows (Win32 API), macOS (pbcopy), Linux (xclip/xsel)
- **Privilege Elevation**: Windows (runas), Unix (sudo)
- **HTTP Server**: Raw BSD sockets, static file serving with MIME detection
- **Frontend**: Vanilla HTML/CSS/JS, no frameworks
- **WebGL** (Beta): WebGL 1.0, GLSL shaders for force-directed graph

## Changelog

### v1.1.0
- WebUI lazy-loading: single-layer API, async directory expansion
- Frontend file separation: extracted into `src/web/`
- Multi-language support: Chinese / English
- WebGL rendering (Beta): force-directed graph visualization
- CSS overhaul: variables, responsive design, animations
- Settings panel with hidden files toggle
- Static file serving with fallback to embedded HTML

### v1.0.2
- Initial WebUI release

### v1.0.1
- First public release

## Contributing

Issues and Pull Requests are welcome. Please ensure code style consistency and pass cross-platform tests.

## License

MIT License © 2026 WinTerminal