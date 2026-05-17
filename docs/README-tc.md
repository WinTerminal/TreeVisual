[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/)
[![MIT](https://img.shields.io/badge/License-MIT-green?style=flat)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey?style=flat)](https://github.com/WinTerminal/TreeVisual)
[![Version](https://img.shields.io/badge/version-v1.1.4-7aa2f7?style=flat)]()
[![Release](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml/badge.svg)](https://github.com/WinTerminal/TreeVisual/actions/workflows/release.yml)

**[English](../README.md) | [简体中文](README-zh.md) | [繁體中文](README-tc.md)**

<p align="center">
  <img src="../TreeViz-logo.png" alt="TreeVisual Logo" width="128" height="128">&nbsp;&nbsp;<img src="../TreeViz-logo-light.png" alt="TreeVisual Logo" width="128" height="128">
  <br>
  <sub>深色模式 &nbsp;|&nbsp; 淺色模式</sub>
</p>

# TreeVisual - 跨平台目錄樹視覺化工具

現代化的目錄樹工具，支援 TUI 模式和 **WebUI**（懶加載樹瀏覽器 + Canvas 硬體加速渲染）。基於 C++17 建置，零第三方相依性。
[下載最新版本](https://github.com/WinTerminal/TreeVisual/releases/latest) | 透過 GitHub Actions 自動建置。  
[線上預覽](https://winterminal.github.io/TreeVisual/)（靜態介面）

## 功能特性

### CLI / TUI 模式
- **美觀的樹形輸出** - Unicode 連線字元，層次清晰
- **智慧輸出策略** - 終端機顯示 / 剪貼簿 / 檔案自動切換
- **多執行緒加速** - 平行走訪大型目錄（2-5 倍提速）
- **自動提權** - 權限不足時提示以管理員/root 身份重新執行
- **安全處理符號連結** - 防止循環連結導致崩潰
- **守護程式服務模式** (`--service on|off|status`) - 背景 WebUI 服務

### WebUI 模式 (`--web`)
- **懶加載樹結構** - 按需展開目錄，僅載入當前層級
- **Canvas 2D 硬體加速渲染** - 字元樹形展示，平滑展開/收起動畫，Unicode 方框繪製
- **4 種主題 × 2 種模式**（Mocha、Macchiato、Frappé、Latte × 深色/淺色）— 共 8 套配色方案
- **路徑自動補全** - 防抖、大小寫不敏感前綴匹配，目錄優先排序
- **國際化支援** - 英文 / 簡體中文，自動偵測瀏覽器語言
- **設定面板** - 字型大小、字型族、主題、動畫速度、隱藏檔案、服務開關
- **展開/收起動畫** - 漸入+滑動交錯動畫（打開和關閉均支援）
- **自訂顏色** - 自選背景色、文字色、目錄色、根目錄色
- **JS 模式（Beta）** - 無需後端，使用瀏覽器 File System API 存取本機檔案系統
- **GitHub Pages 預覽** — 靜態介面可存取 [winterminal.github.io/TreeVisual](https://winterminal.github.io/TreeVisual/)

## 快速開始

```bash
# 方式一：下載預編譯套件
# https://github.com/WinTerminal/TreeVisual/releases/latest

# 方式二：從原始碼建置（自動下載相依性）
./install.sh    # Linux/macOS
install.bat     # Windows

# 方式三：手動編譯
git clone https://github.com/WinTerminal/TreeVisual.git
cd TreeVisual && mkdir build && cd build
cmake .. && make -j$(nproc)
./tree --web
```

安裝腳本會自動下載原始碼和 Web 資源，將其嵌入二進位檔，編譯並清理。  
腳本會偵測系統地區設定和編碼（UTF-8/GBK/Big5），以英文、簡體中文或繁體中文顯示訊息。

## 使用方法

```
tree [路徑]              顯示目錄樹（CLI）
tree -v                  互動式 TUI 模式
tree --web               啟動 WebUI 伺服器（埠號 7200）
tree --web stop          停止執行中的 WebUI 伺服器
tree --service on        啟動背景守護程式
tree --service off       停止守護程式
tree --service status    檢查守護程式狀態
tree --hidden            顯示隱藏檔案/目錄
tree --setting           開啟設定面板
```

## 架構

```
TreeVisual/
├── src/
│   ├── tree.cpp          # 主原始碼檔案（約 1860 行）
│   └── web/              # WebUI 靜態資源
│       ├── index.html
│       ├── styles.css    # 4 種 Catppuccin 主題，深色/淺色模式
│       ├── app.js        # 核心邏輯（樹形渲染、懶加載、設定、服務）
│       ├── i18n.js       # 英/中詞典
│       └── webgl.js      # Canvas 2D 硬體加速字元樹渲染器
├── Preview/              # GitHub Pages 部署副本
├── scripts/
│   └── embed-web.py      # 編譯時將 web/ 嵌入 tree.cpp
├── packaging/            | 發布打包範本
├── .github/workflows/
│   ├── build.yml         # 推送/PR 時 CI 建置
│   ├── release.yml       # 標籤推送時自動發布（3 平台）
│   └── pages.yml         # 部署 Web UI 到 GitHub Pages
├── CMakeLists.txt
├── install.sh            # Linux/macOS 安裝腳本（自動下載+編譯）
└── install.bat           # Windows 安裝腳本
```

## API

| 端點 | 方法 | 參數 | 說明 |
|------|------|------|------|
| `/api/tree` | GET | `path`, `show_hidden` | 單層樹 JSON |
| `/api/list` | GET | `path`, `show_hidden` | 扁平目錄列表 JSON |
| `/api/settings` | GET/POST | - | 持久化設定（主題、模式、字型等） |
| `/api/home` | GET | - | 傳回使用者主目錄路徑 |
| `/api/service/status` | GET | - | 守護程式執行狀態 |
| `/api/service/start` | POST | - | 背景啟動守護程式 |
| `/api/service/stop` | POST | - | 停止執行中的守護程式 |

## 技術棧

| 層級 | 技術 |
|------|------|
| 語言 | C++17 |
| 檔案系統 | `std::filesystem` |
| 並行 | `std::thread`, `std::atomic` |
| HTTP 伺服器 | 原生 BSD socket（零相依性） |
| 前端 | 原生 HTML/CSS/JS（無框架） |
| Canvas | Canvas 2D `getContext('2d')` — 硬體加速字元樹 |
| 剪貼簿 | Win32 API / pbcopy / xclip |
| CI/CD | GitHub Actions（3 平台） |

## 更新日誌

### v1.1.4
- **收起動畫** — 展開和收起均支援交錯滑動+漸變動畫
- **動畫優化** — 緩動函數從 easeOutQuad 改為 easeOutCubic；全新實現收起動畫
- **服務模式修復** — `isRunning()` 正確偵測殭屍程序；`stop()` 支援自殺保護；埠號共用 `SO_REUSEPORT`
- **前端服務開關修復** — 點擊 Stop 後介面及時更新為 Off（catch 處理器處理連線中斷）
- **GitHub Pages 預覽** — `Preview/` 目錄 + `pages.yml` 工作流程，靜態介面線上預覽
- **版本號自動同步** — Release 工作流程自動更新 `index.html` 和 `README.md` 版本號並推送

### v1.1.3
- **自動發布工作流程**：標籤推送觸發跨平台建置、打包和 GitHub Release
- **安裝腳本增強**：自動下載 Web 資源，嵌入二進位檔，編譯後清理
- **國際化：3 種語言**：英文 / 簡體中文 / 繁體中文，自動偵測地區編碼（UTF-8/GBK/Big5）
- **PATH 提示**：預設 `(Y/n)` — Enter 確認新增，`n` 跳過
- **WebUI 內嵌回退**：`WEB_EMBEDDED` 編譯旗標將 Web 檔案烘焙進二進位檔

### v1.1.2
- **Bug 修復：14 個問題修復**：Windows Service 崩潰（條件反轉）、硬編碼主目錄路徑、CSS 孤兒規則、MSVC 字串限制、CSP 標頭等
- **跨平台套件**：透過 GitHub Actions 提供 Linux/macOS/Windows 預編譯二進位檔

### v1.1.1
- 服務模式 (`--service on|off|status`)
- 設定持久化 (`/api/settings`)
- WebUI 啟停子指令 (`--web start|stop`)

### v1.1.0
- WebUI 懶加載，前端分離至 `src/web/`
- 國際化支援（中/英）
- WebGL 測試版，CSS 重構，設定面板
- 靜態檔案服務及回退機制

## 貢獻

歡迎提交 Issue 和 Pull Request。請確保跨平台相容性。

## 授權條款

MIT © 2026 WinTerminal
