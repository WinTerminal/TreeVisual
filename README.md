[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.4-7aa2f7?style=flat)]()
[![Release](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml)

<p align="center">
  <img src="TreeViz-logo.png" alt="TreeVisual Logo (Dark)" width="128" height="128">&nbsp;&nbsp;<img src="TreeViz-logo-light.png" alt="TreeVisual Logo (Light)" width="128" height="128">
  <br>
  <sub>Dark Mode &nbsp;|&nbsp; Light Mode</sub>
</p>

<h1 align="center">TreeVisual</h1>
<p align="center"><i>Cross-platform Directory Tree Visualizer — 跨平台目錄樹視覺化工具</i></p>

<p align="center">
  <b><a href="docs/README.md">📖 English</a></b> &nbsp;|&nbsp;
  <b><a href="docs/README-zh.md">📖 简体中文</a></b> &nbsp;|&nbsp;
  <b><a href="docs/README-tc.md">📖 繁體中文</a></b>
</p>

<p align="center">
  <a href="https://github.com/WinTerminal/TreeVisual/releases/latest"><b>⬇️ Download Release</b></a> &nbsp;•&nbsp;
  <a href="https://winterminal.github.io/TreeVisual/"><b>🌐 Live Preview</b></a> &nbsp;•&nbsp;
  <a href="#quick-start"><b>🚀 Quick Start</b></a>
</p>

---

## Quick Start

```bash
# Download pre-built package
# https://github.com/WinTerminal/TreeVisual/releases/latest

# Or build from source
git clone https://github.com/WinTerminal/TreeVisual.git && cd TreeVisual
./install.sh    # Linux/macOS
install.bat     # Windows

./tree --web     # Start WebUI at http://127.0.0.1:7200/
```

## Features

- **CLI / TUI** — Beautiful Unicode tree output, multi-threaded parallel scan, auto privilege elevation
- **WebUI** — Lazy-loading tree browser with Canvas HW-accelerated rendering, 8 color themes, i18n (EN/zh)
- **JS Mode (Beta)** — Browser File System API, no backend required
- **Cross-platform** — Linux / macOS / Windows, zero third-party dependencies
- **GitHub Pages** — [Live Demo](https://winterminal.github.io/TreeVisual/) available now

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Backend | C++17, `std::filesystem`, `std::thread`, `std::atomic` |
| HTTP | Raw BSD sockets (zero deps) |
| Frontend | Vanilla HTML/CSS/JS, Canvas 2D HW acceleration |
| CI/CD | GitHub Actions (3 platforms auto-release) |

## License

[MIT](LICENSE) © 2026 [WinTerminal](https://github.com/WinTerminal)
