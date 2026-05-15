TreeVisual {version} Usage Guide
==========================

Features
- Beautiful directory tree output with Unicode/ASCII branches
- Multi-threaded parallel scanning (2-5x speedup)
- WebUI mode with lazy-loading tree browser (--web)
- TUI interactive mode (-v)
- i18n support (Chinese/English)
- WebGL force-directed graph visualization (Beta)
- Auto clipboard copy or file save for large trees
- Hidden files toggle (--hidden)
- Daemon service mode (--service)

Usage
  ./tree [path] [options]

Options
  --hidden         Show hidden files
  -v               Interactive TUI mode
  -w, --web        Start WebUI server (http://127.0.0.1:7200/)
  --service [on|off|status]  Background daemon mode
  --setting        Open settings panel
  -h, --help       Show help

Examples
  ./tree                     Current directory tree
  ./tree /path/to/dir        Tree of specific directory
  ./tree --web               Start WebUI with browser interface
  ./tree -v                  Interactive terminal UI
  ./tree --hidden            Include hidden files

License
MIT License (c) 2026 WinTerminal
