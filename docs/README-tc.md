[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)

[English](./README.md) | [简体中文](./README-zh.md) | [繁體中文](./README-tc.md)

# TreeVisual - 跨平台智慧目錄樹工具

一個現代化的命令列工具，用於產生美觀的目錄樹結構。採用物件導向架構設計，具備智慧輸出策略、多執行緒平行掃描、自动提權等特性。

## 特性

- **美觀樹形輸出** - 使用 Unicode 連線字元，層次清晰易讀
- **智慧輸出策略**：
  - ≤50 行：終端顯示 + 自動複製到剪貼簿
  - 51-100 行：僅複製到剪貼簿（避免刷屏）
  - >100 行：自動儲存為檔案
- **多執行緒加速** - 同一層級子目錄平行遍歷，大型目錄提速 2-5 倍
- **自動提權** - 遇到權限不足時，自動詢問是否以管理員/root 權限重新執行
- **安全處理符號連結** - 避免循環連結導致的崩潰
- **跨平台支援** - Linux、macOS、Windows 執行一致

## 架構設計

```
┌─────────────────────────────────────────────────┐
│                    App                         │
│               (主應用入口)                      │
└─────────────────────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           ▼                     ▼
    ┌──────────────┐    ┌─────────────────┐
    │  Directory  │    │ IPlatformHelper │
    │    Tree    │    │ (平台介面)      │
    └──────────────┘    └─────────────────┘
           │                    │
    ┌─────┴─────┐       ┌─────┴─────┐
    ▼           ▼       ▼           ▼
 ITraversal  Node   Windows   Unix
 Policy            Helper    Helper
```

### 核心類

| 類名 | 說明 |
|------|------|
| `PermissionErrorTracker` | 單例，執行緒安全地記錄權限錯誤 |
| `Node` | 目錄樹節點，支援檔案和目錄類型 |
| `ITraversalPolicy` | 遍歷策略介面，支援自訂實作 |
| `DefaultTraversalPolicy` | 預設遍歷實作，過濾隱藏檔案 |
| `DirectoryTree` | 序列目錄樹建構器 |
| `ParallelDirectoryTree` | 平行目錄樹建構器，繼承自 `DirectoryTree` |
| `IOutputHandler` | 輸出處理介面 |
| `ConsoleOutputHandler` | 終端輸出 |
| `ClipboardOutputHandler` | 剪貼簿輸出 |
| `FileOutputHandler` | 檔案輸出 |
| `IPlatformHelper` | 平台助手介面 |
| `WindowsPlatformHelper` | Windows 平台實作 |
| `UnixPlatformHelper` | Unix/Linux/macOS 平台實作 |
| `TreeFormatter` | 樹格式化器，將 Node 轉換為文字 |
| `App` | 主應用類，協調各元件 |

### 設計模式

- **單例模式**：`PermissionErrorTracker`
- **策略模式**：`ITraversalPolicy`、`IOutputHandler`、`IPlatformHelper`
- **模板方法模式**：`DirectoryTree::build()` → `buildNode()`
- **繼承多態**：`ParallelDirectoryTree` 覆寫 `build()` 方法實現平行

## 安裝

### 方式一：安裝腳本（推薦）

專案已自帶安裝腳本，自動處理依賴安裝、編譯和 PATH 設定。

```bash
# Linux/macOS
./install.sh

# Windows (以管理員身份執行)
install.bat
```

安裝腳本功能：
- Linux: 自動安裝 `xclip` 剪貼簿依賴
- 編譯 TreeVisual
- 檢測系統 `tree` 命令衝突，自動重新命名為 `treeviz`
- 互動式引導添加到 PATH

### 方式二：預編譯二進制

從 [Releases](https://github.com/WinTerminal/TreeVisual/releases) 下載對應平台的可執行檔。

### 方式三：CMake 編譯

**依賴**：CMake 3.10+

```bash
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual
mkdir build && cd build
cmake ..
cmake --build . --config Release
sudo cmake --install .
```

### 方式四：直接編譯

**依賴**：C++17 編譯器（g++/clang++），Linux 需 `xclip` 或 `xsel`

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree
```

Windows (MinGW)：

```bash
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree.exe -static -lshlwapi -lole32 -lshell32
```

## 使用方法

```bash
# 顯示目前目錄樹
tree

# 顯示指定目錄
tree /path/to/directory

# 掃描使用者主目錄
tree ~

# 顯示隱藏檔案
tree --hidden
```

## 技術棧

- **語言**：C++17
- **標準庫**：`std::filesystem`、`std::thread`、`std::atomic`
- **剪貼簿**：Windows (Win32 API)，macOS (pbcopy)，Linux (xclip/xsel)
- **提權**：Windows (runas)，Unix (sudo)

## 貢獻

歡迎 Issue 和 Pull Request。請確保程式碼風格一致並通過跨平台測試。

## 授權

MIT License © 2026 WinTerminal