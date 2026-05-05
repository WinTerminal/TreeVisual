[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)

[English](./README.md) | [简体中文](./README-zh.md) | [繁體中文](./README-tc.md)

# TreeVisual - Cross-platform Intelligent Directory Tree Tool

A modern command-line tool for generating beautiful directory tree structures. Built with object-oriented architecture, featuring intelligent output strategies, multi-threaded parallel scanning, and automatic privilege elevation.

## Features

- **Beautiful tree output** - Unicode branch characters, clear hierarchy
- **Intelligent output strategies**:
  - ≤50 lines: display in terminal + copy to clipboard
  - 51-100 lines: copy to clipboard only (prevent flooding)
  - >100 lines: auto-save to file
- **Multi-threaded acceleration** - Parallel traversal of same-level subdirectories, 2-5x speedup for large directories
- **Auto privilege elevation** - When permission denied, prompt to re-run as admin/root
- **Safe symlink handling** - Prevent crashes from circular links
- **Cross-platform** - Works consistently on Linux, macOS, Windows

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

# TUI mode
tree -v

# Settings
tree --setting
```

## Tech Stack

- **Language**: C++17
- **Standard Library**: `std::filesystem`, `std::thread`, `std::atomic`
- **Clipboard**: Windows (Win32 API), macOS (pbcopy), Linux (xclip/xsel)
- **Privilege Elevation**: Windows (runas), Unix (sudo)

## Contributing

Issues and Pull Requests are welcome. Please ensure code style consistency and pass cross-platform tests.

## License

MIT License © 2026 WinTerminal