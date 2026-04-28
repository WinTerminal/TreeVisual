#!/bin/bash

#TreeVisual 安装脚本 (Linux/macOS)

#功能: 安装依赖 xclip (Linux)、编译 tree、配置 PATH

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

OS="$(uname -s)"
case "$OS" in
Linux) PLATFORM="linux" ;;
Darwin) PLATFORM="macos" ;;
*)
  echo -e "${RED}不支持的操作系统: $OS${NC}"
  exit 1
  ;;
esac
echo -e "${GREEN}检测到平台: $PLATFORM${NC}"

#---------- 1. 安装依赖 (仅 Linux) ----------

if [ "$PLATFORM" = "linux" ]; then
  if command -v xclip &>/dev/null || command -v xsel &>/dev/null; then
    echo -e "${GREEN}xclip/xsel 已安装，跳过依赖安装${NC}"
  else
    echo -e "${YELLOW}未检测到 xclip 或 xsel，正在尝试安装...${NC}"
    if command -v apt &>/dev/null; then
      sudo apt update && sudo apt install -y xclip
    elif command -v yum &>/dev/null; then
      sudo yum install -y xclip
    elif command -v dnf &>/dev/null; then
      sudo dnf install -y xclip
    elif command -v pacman &>/dev/null; then
      sudo pacman -S --noconfirm xclip
    elif command -v zypper &>/dev/null; then
      sudo zypper install -y xclip
    else
      echo -e "${RED}未找到包管理器，尝试从源码编译 xclip...${NC}"
      if ! command -v gcc &>/dev/null; then
        echo -e "${RED}缺少编译工具 gcc，请手动安装 xclip 后重试。${NC}"
        exit 1
      fi
      cd /tmp
      git clone https://github.com/astrand/xclip.git
      cd xclip
      ./bootstrap
      ./configure
      make
      sudo make install
      cd -
    fi
    if command -v xclip &>/dev/null || command -v xsel &>/dev/null; then
      echo -e "${GREEN}xclip/xsel 安装成功${NC}"
    else
      echo -e "${RED}xclip/xsel 安装失败，剪贴板功能将不可用。${NC}"
    fi
  fi
fi

#---------- 2. 编译 ----------

BINARY_NAME="tree"
if command -v tree &>/dev/null; then
  echo -e "${YELLOW}检测到系统已安装 tree 命令，将编译输出命名为 treeviz${NC}"
  BINARY_NAME="treeviz"
fi

echo -e "${GREEN}开始编译 TreeVisual...${NC}"
if [ -f "CMakeLists.txt" ]; then
  mkdir -p build
  cd build
  cmake ..
  make -j$(nproc)
  cd ..
  cp build/tree "./$BINARY_NAME"
else
  g++ -std=c++17 -pthread -O2 src/tree.cpp -o "$BINARY_NAME"
fi
echo -e "${GREEN}编译完成，可执行文件: ./$BINARY_NAME${NC}"

#---------- 3. 配置 PATH ----------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_PATH="$SCRIPT_DIR"

if echo "$PATH" | grep -q "$BIN_PATH"; then
  echo -e "${GREEN}当前目录已在 PATH 中，无需配置。${NC}"
else
  echo -e "${YELLOW}当前目录 ($BIN_PATH) 不在 PATH 中。${NC}"
  read -p "是否自动添加到 PATH？(y/n): " -n 1 -r
  echo
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    SHELL_NAME="$(basename "$SHELL")"
    CONFIG_FILE=""
    case "$SHELL_NAME" in
    bash) CONFIG_FILE="$HOME/.bashrc" ;;
    zsh) CONFIG_FILE="$HOME/.zshrc" ;;
    fish) CONFIG_FILE="$HOME/.config/fish/config.fish" ;;
    *)
      echo -e "${RED}未识别的 shell: $SHELL_NAME，请手动添加 PATH。${NC}"
      exit 0
      ;;
    esac
    cp "$CONFIG_FILE" "$CONFIG_FILE.bak"
    echo "export PATH=\"$BIN_PATH:\$PATH\"" >>"$CONFIG_FILE"
    echo -e "${GREEN}已添加 PATH 到 $CONFIG_FILE${NC}"
    echo -e "${YELLOW}请执行以下命令使配置生效: source $CONFIG_FILE${NC}"
    export PATH="$BIN_PATH:$PATH"
  else
    echo -e "${YELLOW}跳过 PATH 配置。${NC}"
  fi
fi

echo -e "${GREEN}安装完成！现在可以使用 'tree' 命令。${NC}"
