@echo off
setlocal EnableDelayedExpansion

:: =============================================
:: TreeVisual Installation Script (Windows)
:: =============================================

title TreeVisual Installer v1.1.0

set "BINARY_NAME=tree.exe"
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

echo =======================================
echo   TreeVisual Installer v1.1.0
echo =======================================
echo.

::---------- Detect System tree ----------
where tree >nul 2>&1
if %errorlevel% equ 0 (
    echo [WARNING] System has tree command, renaming to treeviz.exe
    set "BINARY_NAME=treeviz.exe"
)

::---------- Check Compiler ----------
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] No compiler found.
    echo Please use portable version: https://github.com/WinTerminal/TreeVisual/releases
    pause
    exit /b 1
)

::---------- Download Source ----------
if not exist "src" mkdir src 2>nul
if not exist "src\tree.cpp" (
    echo [INFO] Downloading source code...
    powershell -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/WinTerminal/TreeVisual/main/src/tree.cpp' -OutFile 'src\tree.cpp'"
    if exist "src\tree.cpp" (
        echo [OK] Download complete
    ) else (
        echo [ERROR] Download failed
    )
)

::---------- Embed Web Assets ----------
set "EMBED_FLAG="
if exist "scripts\embed-web.py" if exist "src\web" (
    echo [INFO] Embedding web assets into binary...
    python3 scripts\embed-web.py
    if !errorlevel! equ 0 (
        set "EMBED_FLAG=/DWEB_EMBEDDED"
    ) else (
        echo [WARN] Web embedding failed, continuing without embedded assets
    )
)

::---------- Compile ----------
echo [INFO] Compiling TreeVisual...

if exist "CMakeLists.txt" (
    echo [INFO] Using CMake
    if not exist "build" mkdir build
    cd build
    if "!EMBED_FLAG!"=="" (
        cmake .. 2>&1
    ) else (
        cmake .. -DWEB_EMBEDDED=ON 2>&1
    )
    cmake --build . --config Release 2>&1
    cd ..
    copy "build\Release\tree.exe" "%BINARY_NAME%" 2>&1
) else (
    echo [INFO] Using g++
    g++ -std=c++17 -pthread -O2 !EMBED_FLAG! src/tree.cpp -o "%BINARY_NAME%" -static -lshlwapi -lole32 -lshell32
)

if not exist "%BINARY_NAME%" (
    echo [ERROR] Build failed
    pause
    exit /b 1
)

echo [OK] Build complete: .\%BINARY_NAME%

::---------- Copy WebUI Assets ----------
if exist "src\web" (
    if not exist "share\treevisual\web" mkdir share\treevisual\web >nul 2>&1
    xcopy /E /Y /Q "src\web\*" "share\treevisual\web\" >nul 2>&1
    if errorlevel 1 (
        echo [WARN] Failed to copy WebUI assets
    ) else (
        echo [OK] WebUI assets copied to share\treevisual\web\
    )
) else (
    echo [INFO] No src\web directory found, skipping WebUI assets
)

echo.

::---------- Configure PATH ----------
echo %PATH% | findstr /I /C:"%SCRIPT_DIR%" >nul
if %errorlevel% equ 0 (
    echo [OK] Directory already in PATH
    goto :complete
)

echo [INFO] Add to system PATH? (y/n)
set /p "add_path="
if /i not "!add_path!"=="y" (
    echo [INFO] Skipped PATH configuration
    echo You can manually add: %SCRIPT_DIR%
    goto :complete
)

echo [INFO] Requesting administrator privileges...
powershell -Command "Start-Process cmd -Verb RunAs -ArgumentList '/c setx PATH \"%SCRIPT_DIR%;%PATH%\" /M' -Wait"
if %errorlevel% equ 0 (
    echo [OK] Added to system PATH
    echo Please restart your command prompt
) else (
    echo [ERROR] Failed to add to PATH
)

:complete
echo.
echo =======================================
echo   Installation complete!
echo   Use: .\%BINARY_NAME%
echo =======================================
pause