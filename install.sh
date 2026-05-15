#!/bin/bash

#=============================================
# TreeVisual Installation Script (Linux/macOS)
#=============================================
# Features: Install dependencies, compile tree, configure PATH
# Language: Auto-detect (English/Chinese)

set -e

#---------- Configuration ----------
SCRIPT_VERSION="1.1.0"
BINARY_NAME="tree"

#---------- Color Output ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

#---------- Detect Language & Encoding ----------
# Extract encoding from locale (e.g. zh_CN.UTF-8 -> UTF-8, zh_CN.GBK -> GBK)
ENCODING=""
if [[ "$LANG" =~ \.([^.]*)$ ]]; then
    ENCODING="${BASH_REMATCH[1]}"
fi

if [[ -z "$ENCODING" ]] || [[ "$ENCODING" =~ ^[Uu][Tt][Ff]-?8$ ]] || [[ "$ENCODING" =~ ^[Uu][Tt][Ff]8$ ]]; then
    # UTF-8 or unknown encoding: use language-based selection safely
    if [[ "$LANG" =~ ^zh_CN ]]; then
        SCRIPT_LANG="cn"
    elif [[ "$LANG" =~ ^zh_TW ]] || [[ "$LANG" =~ ^zh_HK ]] || [[ "$LANG" =~ ^zh_SG ]]; then
        SCRIPT_LANG="tw"
    else
        SCRIPT_LANG="en"
    fi
else
    # Non-UTF-8 encoding (GBK, Big5, etc.): force English to avoid mojibake
    SCRIPT_LANG="en"
fi

#---------- Messages ----------
msg() {
    case "$SCRIPT_LANG" in
        cn)
            case "$1" in
                init) echo -e "${BLUE}TreeVisual 安装脚本 v${SCRIPT_VERSION}${NC}" ;;
                platform) echo -e "${GREEN}检测到平台: $2${NC}" ;;
                dep_exists) echo -e "${GREEN}$2 已安装，跳过${NC}" ;;
                dep_installing) echo -e "${YELLOW}正在安装 $2...${NC}" ;;
                dep_failed) echo -e "${RED}$2 安装失败${NC}" ;;
                dep_success) echo -e "${GREEN}$2 安装成功${NC}" ;;
                no_pkgmgr) echo -e "${RED}未找到包管理器${NC}" ;;
                downloading) echo -e "${YELLOW}正在下载源码: $2${NC}" ;;
                success) echo -e "${GREEN}成功${NC}" ;;
                failed) echo -e "${RED}失败: $2${NC}" ;;
                compiling) echo -e "${GREEN}开始编译 TreeVisual...${NC}" ;;
                build_cmake) echo -e "使用 CMake 编译" ;;
                build_gxx) echo -e "使用 g++ 编译" ;;
                build_success) echo -e "${GREEN}编译完成: ./$2${NC}" ;;
                build_failed) echo -e "${RED}编译失败${NC}" ;;
                tree_exists) echo -e "${YELLOW}系统已安装 tree，重命名为 $2${NC}" ;;
                path_exists) echo -e "${GREEN}目录已在 PATH 中${NC}" ;;
                path_ask) echo -e "${YELLOW}是否添加到 PATH？(Y/n): ${NC}" ;;
                path_added) echo -e "${GREEN}已添加 PATH 到 $2${NC}" ;;
                path_skip) echo -e "${YELLOW}跳过 PATH 配置${NC}" ;;
                path_shell_err) echo -e "${RED}未识别的 shell: $2${NC}" ;;
                complete) echo -e "${GREEN}安装完成！$2${NC}" ;;
                unsupported) echo -e "${RED}不支持的操作系统: $2${NC}" ;;
            esac
            ;;
        tw)
            case "$1" in
                init) echo -e "${BLUE}TreeVisual 安裝腳本 v${SCRIPT_VERSION}${NC}" ;;
                platform) echo -e "${GREEN}偵測到平台: $2${NC}" ;;
                dep_exists) echo -e "${GREEN}$2 已安裝，跳過${NC}" ;;
                dep_installing) echo -e "${YELLOW}正在安裝 $2...${NC}" ;;
                dep_failed) echo -e "${RED}$2 安裝失敗${NC}" ;;
                dep_success) echo -e "${GREEN}$2 安裝成功${NC}" ;;
                no_pkgmgr) echo -e "${RED}找不到套件管理器${NC}" ;;
                downloading) echo -e "${YELLOW}正在下載: $2${NC}" ;;
                success) echo -e "${GREEN}成功${NC}" ;;
                failed) echo -e "${RED}失敗: $2${NC}" ;;
                compiling) echo -e "${GREEN}開始編譯 TreeVisual...${NC}" ;;
                build_cmake) echo -e "使用 CMake 編譯" ;;
                build_gxx) echo -e "使用 g++ 編譯" ;;
                build_success) echo -e "${GREEN}編譯完成: ./$2${NC}" ;;
                build_failed) echo -e "${RED}編譯失敗${NC}" ;;
                tree_exists) echo -e "${YELLOW}系統已安裝 tree，重新命名為 $2${NC}" ;;
                path_exists) echo -e "${GREEN}目錄已在 PATH 中${NC}" ;;
                path_ask) echo -e "${YELLOW}是否加入 PATH？(Y/n): ${NC}" ;;
                path_added) echo -e "${GREEN}已加入 PATH 到 $2${NC}" ;;
                path_skip) echo -e "${YELLOW}跳過 PATH 設定${NC}" ;;
                path_shell_err) echo -e "${RED}無法識別的 shell: $2${NC}" ;;
                complete) echo -e "${GREEN}安裝完成！$2${NC}" ;;
                unsupported) echo -e "${RED}不支援的作業系統: $2${NC}" ;;
            esac
            ;;
        en)
            case "$1" in
                init) echo -e "${BLUE}TreeVisual Installation Script v${SCRIPT_VERSION}${NC}" ;;
                platform) echo -e "${GREEN}Detected platform: $2${NC}" ;;
                dep_exists) echo -e "${GREEN}$2 already installed, skipping${NC}" ;;
                dep_installing) echo -e "${YELLOW}Installing $2...${NC}" ;;
                dep_failed) echo -e "${RED}$2 installation failed${NC}" ;;
                dep_success) echo -e "${GREEN}$2 installed successfully${NC}" ;;
                no_pkgmgr) echo -e "${RED}No package manager found${NC}" ;;
                downloading) echo -e "${YELLOW}Downloading source: $2${NC}" ;;
                success) echo -e "${GREEN}Success${NC}" ;;
                failed) echo -e "${RED}Failed: $2${NC}" ;;
                compiling) echo -e "${GREEN}Compiling TreeVisual...${NC}" ;;
                build_cmake) echo -e "Using CMake" ;;
                build_gxx) echo -e "Using g++" ;;
                build_success) echo -e "${GREEN}Build complete: ./$2${NC}" ;;
                build_failed) echo -e "${RED}Build failed${NC}" ;;
                tree_exists) echo -e "${YELLOW}System has tree, renamed to $2${NC}" ;;
                path_exists) echo -e "${GREEN}Directory already in PATH${NC}" ;;
                path_ask) echo -e "${YELLOW}Add to PATH？(Y/n): ${NC}" ;;
                path_added) echo -e "${GREEN}Added PATH to $2${NC}" ;;
                path_skip) echo -e "${YELLOW}Skipped PATH configuration${NC}" ;;
                path_shell_err) echo -e "${RED}Unknown shell: $2${NC}" ;;
                complete) echo -e "${GREEN}Installation complete! $2${NC}" ;;
                unsupported) echo -e "${RED}Unsupported OS: $2${NC}" ;;
            esac
            ;;
    esac
}

#---------- Platform Detection ----------
detect_platform() {
    local os
    os=$(uname -s)
    case "$os" in
        Linux) echo "linux" ;;
        Darwin) echo "macos" ;;
        *)
            msg unsupported "$os"
            exit 1
            ;;
    esac
}

#---------- Install Dependencies ----------
install_dependencies() {
    local pkg_mgr
    local packages

    if [[ "$PLATFORM" == "macos" ]]; then
        msg dep_exists "pbcopy (built-in)"
        return 0
    fi

    if command -v xclip &>/dev/null || command -v xsel &>/dev/null; then
        msg dep_exists "xclip/xsel"
        return 0
    fi

    msg dep_installing "xclip"

    if command -v apt &>/dev/null; then
        sudo apt update && sudo apt install -y xclip
        pkg_mgr="apt"
    elif command -v yum &>/dev/null; then
        sudo yum install -y xclip
        pkg_mgr="yum"
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y xclip
        pkg_mgr="dnf"
    elif command -v pacman &>/dev/null; then
        sudo pacman -S --noconfirm xclip
        pkg_mgr="pacman"
    elif command -v zypper &>/dev/null; then
        sudo zypper install -y xclip
        pkg_mgr="zypper"
    else
        msg no_pkgmgr
        if command -v gcc &>/dev/null; then
            msg dep_installing "xclip from source"
            local tmp_dir="/tmp/xclip-build"
            rm -rf "$tmp_dir"
            mkdir -p "$tmp_dir"
            cd "$tmp_dir"
            git clone --depth 1 https://github.com/astrand/xclip.git .
            ./bootstrap 2>/dev/null || ./configure 2>/dev/null
            make
            sudo make install
            cd - >/dev/null
        else
            msg dep_failed "xclip (no compiler)"
            return 1
        fi
    fi

    if command -v xclip &>/dev/null || command -v xsel &>/dev/null; then
        msg dep_success
    else
        msg dep_failed
        return 1
    fi
}

#---------- Download Source ----------
download_source() {
    if [[ -f "src/tree.cpp" ]]; then
        return 0
    fi

    local url="https://raw.githubusercontent.com/WinTerminal/TreeVisual/main/src/tree.cpp"
    msg downloading "$url"

    if command -v curl &>/dev/null; then
        curl -fsSL "$url" -o src/tree.cpp
    elif command -v wget &>/dev/null; then
        wget -q "$url" -O src/tree.cpp
    else
        msg failed "curl/wget"
        return 1
    fi

    if [[ -f "src/tree.cpp" ]]; then
        msg success
    else
        return 1
    fi
}

#---------- Download Web Assets ----------
download_web() {
    if [[ -d "src/web" ]]; then
        return 0
    fi
    mkdir -p src/web

    local base_url="https://raw.githubusercontent.com/WinTerminal/TreeVisual/main/src/web"
    local files=("index.html" "styles.css" "app.js" "i18n.js" "webgl.js")

    for f in "${files[@]}"; do
        msg downloading "$base_url/$f"
        if command -v curl &>/dev/null; then
            curl -fsSL "$base_url/$f" -o "src/web/$f"
        elif command -v wget &>/dev/null; then
            wget -q "$base_url/$f" -O "src/web/$f"
        else
            msg failed "curl/wget"
            return 1
        fi
    done
    msg success
}

#---------- Compile ----------
compile() {
    if command -v tree &>/dev/null; then
        BINARY_NAME="treeviz"
        msg tree_exists "$BINARY_NAME"
    fi

    download_source
    download_web
    msg compiling

    # Embed web assets into binary (WebUI fallback)
    if command -v python3 &>/dev/null && [[ -f "scripts/embed-web.py" ]] && [[ -d "src/web" ]]; then
        msg dep_installing "web assets"
        python3 scripts/embed-web.py
        EMBED_FLAG="-DWEB_EMBEDDED"
        CMAKE_EMBED="-DWEB_EMBEDDED=ON"
    else
        EMBED_FLAG=""
        CMAKE_EMBED=""
    fi

    if [[ -f "CMakeLists.txt" ]]; then
        msg build_cmake
        mkdir -p build
        cd build
        cmake .. $CMAKE_EMBED >/dev/null 2>&1
        make -j"$(nproc)"
        cd ..
        cp "build/tree" "./$BINARY_NAME"
    else
        msg build_gxx
        g++ -std=c++17 -pthread -O2 $EMBED_FLAG src/tree.cpp -o "$BINARY_NAME"
    fi

    if [[ -f "./$BINARY_NAME" ]]; then
        msg build_success "$BINARY_NAME"
    else
        msg build_failed
        exit 1
    fi

    # Clean up downloaded source and web assets
    if [[ -f "src/tree.cpp" ]]; then rm -f src/tree.cpp; fi
    if [[ -d "src/web" ]]; then rm -rf src/web; fi
}

#---------- Configure PATH ----------
configure_path() {
    local shell_name
    local config_file

    shell_name=$(basename "$SHELL")
    config_file=""

    if echo "$PATH" | grep -q "$BIN_PATH"; then
        msg path_exists
        return 0
    fi

    msg path_ask
    read -n 1 -r
    echo
    if [[ "$REPLY" == "n" ]] || [[ "$REPLY" == "N" ]]; then
        msg path_skip
        return 0
    fi

    case "$shell_name" in
        bash) config_file="$HOME/.bashrc" ;;
        zsh) config_file="$HOME/.zshrc" ;;
        fish) config_file="$HOME/.config/fish/config.fish" ;;
        *)
            msg path_shell_err "$shell_name"
            return 1
            ;;
    esac

    if [[ -f "$config_file" ]]; then
        cp "$config_file" "${config_file}.bak"
    fi
    echo "export PATH=\"$BIN_PATH:\$PATH\"" >> "$config_file"
    msg path_added "$config_file"
    echo "Run: source $config_file"
}

#---------- Main ----------
main() {
    msg init
    PLATFORM=$(detect_platform)
    msg platform "$PLATFORM"

    install_dependencies
    compile

    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    BIN_PATH="$SCRIPT_DIR"

    configure_path

    msg complete "Use './$BINARY_NAME' command"
}

main "$@"