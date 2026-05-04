#!/bin/bash

#=============================================
# TreeVisual Installation Script (Linux/macOS)
#=============================================
# Features: Install dependencies, compile tree, configure PATH
# Language: Auto-detect (English/Chinese)

set -e

#---------- Configuration ----------
SCRIPT_VERSION="1.0.0"
BINARY_NAME="tree"

#---------- Color Output ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

#---------- Detect Language ----------
if [[ "$LANG" =~ ^zh_CN ]]; then
    LANG="cn"
else
    LANG="en"
fi

#---------- Messages ----------
msg() {
    case "$LANG" in
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
                path_ask) echo -e "${YELLOW}是否添加到 PATH？(y/n): ${NC}" ;;
                path_added) echo -e "${GREEN}已添加 PATH 到 $2${NC}" ;;
                path_skip) echo -e "${YELLOW}跳过 PATH 配置${NC}" ;;
                path_shell_err) echo -e "${RED}未识别的 shell: $2${NC}" ;;
                complete) echo -e "${GREEN}安装完成！$2${NC}" ;;
                unsupported) echo -e "${RED}不支持的操作系统: $2${NC}" ;;
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
                path_ask) echo -e "${YELLOW}Add to PATH？(y/n): ${NC}" ;;
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

#---------- Compile ----------
compile() {
    if command -v tree &>/dev/null; then
        BINARY_NAME="treeviz"
        msg tree_exists "$BINARY_NAME"
    fi

    download_source
    msg compiling

    if [[ -f "CMakeLists.txt" ]]; then
        msg build_cmake
        mkdir -p build
        cd build
        cmake .. >/dev/null 2>&1
        make -j"$(nproc)"
        cd ..
        cp "build/tree" "./$BINARY_NAME"
    else
        msg build_gxx
        g++ -std=c++17 -pthread -O2 src/tree.cpp -o "$BINARY_NAME"
    fi

    if [[ -f "./$BINARY_NAME" ]]; then
        msg build_success "$BINARY_NAME"
    else
        msg build_failed
        exit 1
    fi
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
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
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
    source "$config_file"
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