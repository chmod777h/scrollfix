#!/bin/bash
# 编译 scrollfix 和一次性的安装向导，然后启动向导。
# 向导和所有编译产物都放在临时目录里，结束后全部删除。
set -euo pipefail
cd "$(dirname "$0")"

if [ "$(uname)" != "Darwin" ]; then
  echo "仅支持 macOS / macOS only" >&2
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "需要先安装 Xcode 命令行工具，装完后重新运行本脚本。" >&2
  echo "Xcode Command Line Tools are required. Re-run this script after installing them." >&2
  xcode-select --install || true
  exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "正在编译… / Building…"
clang++ -O2 -std=c++17 scrollfix.cpp -framework ApplicationServices -o "$TMP/scrollfix"
clang++ -O2 -std=c++17 setup.cpp -framework CoreFoundation -o "$TMP/scrollfix-setup"

echo "请按屏幕上的向导操作。/ Follow the guide on screen."
"$TMP/scrollfix-setup"
