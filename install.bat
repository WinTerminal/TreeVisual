@echo off
setlocal enabledelayedexpansion
title TreeVisual 安装程序

echo ================================
echo   TreeVisual 安装脚本 (Windows)
echo ================================

echo 正在编译 TreeVisual...
if exist "CMakeLists.txt" (
if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..
copy build\Release\tree.exe .
) else (
g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree.exe -static -lshlwapi -lole32 -lshell32
)
if not exist tree.exe (
echo 编译失败，请检查编译器是否安装。
pause
exit /b 1
)
echo 编译成功。

set "BIN_PATH=%CD%"
echo 当前目录: %BIN_PATH%

echo %PATH% | findstr /I /C:"%BIN_PATH%" >nul
if %errorlevel% equ 0 (
echo 当前目录已在 PATH 中，无需配置。
) else (
echo 当前目录不在 PATH 中。
set /p "add_path=是否自动添加到系统 PATH？(y/n): "
if /i "!add_path!"=="y" (
echo 正在请求管理员权限...
powershell -Command "Start-Process '%~f0' -Verb RunAs -ArgumentList '/addpath'"
exit /b
) else (
echo 跳过 PATH 配置，您可以将 %BIN_PATH% 手动添加到 PATH。
)
)

echo 安装完成！请重新打开命令提示符，即可使用 'tree' 命令。
pause
goto :eof

:addpath
set "BIN_PATH=%~dp0"
set "BIN_PATH=%BIN_PATH:~0,-1%"
echo 正在添加 %BIN_PATH% 到系统 PATH...
setx /M PATH "%BIN_PATH%;%PATH%" >nul
if %errorlevel% equ 0 (
echo 添加成功！请重新打开命令提示符。
) else (
echo 添加失败，请手动添加。
)
pause
