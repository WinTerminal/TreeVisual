// TreeVisual - 跨平台智能目录树工具
// 编译: g++ -std=c++17 -pthread -O2 src/tree.cpp -o tree

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <locale>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
    #include <shlobj.h>
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "user32.lib")
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <cstdlib>
#endif

namespace fs = std::filesystem;

constexpr size_t DISPLAY_LINE_THRESHOLD = 50;
constexpr size_t CLIPBOARD_LINE_THRESHOLD = 100;

// ---------- 线程安全权限错误记录 ----------
std::mutex perm_mutex;
bool has_permission_error = false;
fs::path first_permission_error_path;

void record_permission_error(const fs::path& path) {
    std::lock_guard<std::mutex> lock(perm_mutex);
    if (!has_permission_error) {
        has_permission_error = true;
        first_permission_error_path = path;
    }
}

// ---------- 全局函数声明 ----------
fs::path getHomeDir();
bool isAdmin();
bool runAsAdmin(const std::vector<std::string>& args);
bool copyToClipboard(const std::string& text);
std::string saveToFile(const std::string& content);

// ---------- 安全判断目录（不追踪符号链接）----------
bool is_directory_nofollow(const fs::directory_entry& entry) {
    std::error_code ec;
    auto st = entry.symlink_status(ec);
    if (ec) return false;
    if (fs::is_symlink(st)) return false;
    return fs::is_directory(st);
}

// ---------- 核心并行遍历函数 ----------
std::vector<std::string> traverse_dir_parallel(const fs::path& dir, const std::string& prefix, bool is_last);

std::vector<std::string> build_tree(const fs::path& root) {
    std::vector<std::string> lines;
    std::string root_name = root.filename().string();
    if (root_name.empty()) root_name = root.string();
    lines.push_back(root_name + "/");
    auto children = traverse_dir_parallel(root, "", true);
    lines.insert(lines.end(), children.begin(), children.end());
    return lines;
}

std::vector<std::string> traverse_dir_parallel(const fs::path& dir, const std::string& prefix, bool is_last) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec) {
        record_permission_error(dir);
        return { prefix + "[无权限访问]" };
    }

    std::vector<fs::directory_entry> entries;
    for (auto& entry : it) entries.push_back(entry);

    auto to_lower_str = [](std::string s) -> std::string {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });
        return s;
    };

    std::sort(entries.begin(), entries.end(),
        [&](const fs::directory_entry& a, const fs::directory_entry& b) {
            std::string an = to_lower_str(a.path().filename().string());
            std::string bn = to_lower_str(b.path().filename().string());
            return an < bn;
        });

    std::vector<std::string> result;
    result.reserve(entries.size());

    std::vector<fs::directory_entry> subdirs, files;
    for (const auto& entry : entries) {
        if (is_directory_nofollow(entry))
            subdirs.push_back(entry);
        else
            files.push_back(entry);
    }

    for (const auto& entry : files)
        result.push_back(prefix + "├─ " + entry.path().filename().string());

    const size_t dir_count = subdirs.size();
    if (dir_count == 0) return result;

    auto get_connector = [&](size_t idx) -> std::string {
        bool is_last_item = (idx == dir_count - 1) && files.empty();
        return is_last_item ? "└─ " : "├─ ";
    };

    const size_t PARALLEL_THRESHOLD = 2;
    if (dir_count < PARALLEL_THRESHOLD) {
        for (size_t i = 0; i < dir_count; ++i) {
            const auto& sub = subdirs[i];
            bool sub_is_last = (i == dir_count-1) && files.empty();
            std::string sub_prefix = prefix + (sub_is_last ? "    " : "│   ");
            auto sub_lines = traverse_dir_parallel(sub.path(), sub_prefix, sub_is_last);
            result.insert(result.end(), sub_lines.begin(), sub_lines.end());
        }
        return result;
    }

    size_t hw_concurrency = std::thread::hardware_concurrency();
    const size_t MAX_CONCURRENT_THREADS = (hw_concurrency > 0) ? std::max(size_t(2), hw_concurrency / 2) : size_t(2);
    std::vector<std::vector<std::string>> sub_results(dir_count);
    std::atomic<size_t> next_idx(0);
    std::vector<std::thread> workers;
    workers.reserve(MAX_CONCURRENT_THREADS);

    for (size_t w = 0; w < MAX_CONCURRENT_THREADS; ++w) {
        workers.emplace_back([&]() {
            while (true) {
                size_t idx = next_idx.fetch_add(1);
                if (idx >= dir_count) break;
                const auto& sub = subdirs[idx];
                bool sub_is_last = (idx == dir_count - 1) && files.empty();
                std::string sub_prefix = prefix + (sub_is_last ? "    " : "│   ");
                sub_results[idx] = traverse_dir_parallel(sub.path(), sub_prefix, sub_is_last);
            }
        });
    }

    for (auto& worker : workers) worker.join();

    for (size_t i = 0; i < dir_count; ++i) {
        const auto& sub = subdirs[i];
        std::string connector = get_connector(i);
        result.push_back(prefix + connector + sub.path().filename().string() + "/");
        result.insert(result.end(), sub_results[i].begin(), sub_results[i].end());
    }
    return result;
}

// ---------- 平台具体实现 ----------
fs::path getHomeDir() {
#ifdef _WIN32
    wchar_t* buf = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &buf))) {
        fs::path p(buf);
        CoTaskMemFree(buf);
        return p;
    }
    char* userprofile = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&userprofile, &len, "USERPROFILE");
    if (err == 0 && userprofile && userprofile[0] != '\0') {
        fs::path p(userprofile);
        free(userprofile);
        return p;
    }
    if (userprofile) free(userprofile);
    throw std::runtime_error("无法确定用户主目录 (Windows)");
#else
    const char* home = getenv("HOME");
    if (home && home[0] != '\0') {
        return fs::path(home);
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
        return fs::path(pw->pw_dir);
    }
    throw std::runtime_error("无法确定用户主目录 (Unix)");
#endif
}

bool isAdmin() {
#ifdef _WIN32
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
#else
    return geteuid() == 0;
#endif
}

bool runAsAdmin(const std::vector<std::string>& args) {
#ifdef _WIN32
    std::cout << R"(/$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$         /$$$$$$  /$$$$$$$  /$$      /$$ /$$$$$$ /$$   /$$
 /$$__  $$| $$  | $$|__  $$__//$$__  $$       /$$__  $$| $$__  $$| $$$    /$$$|_  $$_/| $$$ | $$
| $$  \ $$| $$  | $$   | $$  | $$  \ $$      | $$  \ $$| $$  \ $$| $$$$  /$$$$  | $$  | $$$$| $$
| $$$$$$$$| $$  | $$   | $$  | $$  | $$      | $$$$$$$$| $$  | $$| $$ $$/$$ $$  | $$  | $$ $$ $$
| $$__  $$| $$  | $$   | $$  | $$  | $$      | $$__  $$| $$  | $$| $$  $$$| $$  | $$  | $$  $$$$
| $$  | $$| $$  | $$   | $$  | $$  | $$      | $$  | $$| $$  | $$| $$\  $ | $$  | $$  | $$\  $$$
| $$  | $$|  $$$$$$/   | $$  |  $$$$$$/      | $$  | $$| $$$$$$$/| $$ \/  | $$ /$$$$$$| $$ \  $$
|__/  |__/ \______/    |__/   \______/       |__/  |__/|_______/ |__/     |__/|______/|__/  \__/
)" << std::endl;
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring cmdLine;
    for (const auto& a : args) {
        if (!cmdLine.empty()) cmdLine += L" ";
        cmdLine += L"\"" + std::wstring(a.begin(), a.end()) + L"\"";
    }
    HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath, cmdLine.c_str(), nullptr, SW_NORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
#else
    std::cout << R"(/$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$         /$$$$$$  /$$   /$$ /$$$$$$$   /$$$$$$ 
 /$$__  $$| $$  | $$|__  $$__//$$__  $$       /$$__  $$| $$  | $$| $$__  $$ /$$__  $$
| $$  \ $$| $$  | $$   | $$  | $$  \ $$      | $$  \__/| $$  | $$| $$  \ $$| $$  \ $$
| $$$$$$$$| $$  | $$   | $$  | $$  | $$      |  $$$$$$ | $$  | $$| $$  | $$| $$  | $$
| $$__  $$| $$  | $$   | $$  | $$  | $$       \____  $$| $$  | $$| $$  | $$| $$  | $$
| $$  | $$| $$  | $$   | $$  | $$  | $$       /$$  \ $$| $$  | $$| $$  | $$| $$  | $$
| $$  | $$|  $$$$$$/   | $$  |  $$$$$$/      |  $$$$$$/|  $$$$$$/| $$$$$$$/|  $$$$$$/
|__/  |__/ \______/    |__/   \______/        \______/  \______/ |_______/  \______/ 
)" << std::endl;
    std::string exePath = fs::canonical("/proc/self/exe").string();
    std::vector<const char*> argv;
    argv.push_back("sudo");
    argv.push_back(exePath.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    execvp("sudo", const_cast<char* const*>(argv.data()));
    perror("execvp sudo");
    return false;
#endif
}

bool copyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    int utf16size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (utf16size == 0) { CloseClipboard(); return false; }
    HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE, utf16size * sizeof(wchar_t));
    if (!hMem) { CloseClipboard(); return false; }
    wchar_t* wstr = (wchar_t*)GlobalLock(hMem);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wstr, utf16size);
    GlobalUnlock(hMem);
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
#else
    const char* cmd = nullptr;
#ifdef __APPLE__
    cmd = "pbcopy";
#else
    if (system("which xclip >/dev/null 2>&1") == 0)
        cmd = "xclip -selection clipboard";
    else if (system("which xsel >/dev/null 2>&1") == 0)
        cmd = "xsel -i -b";
    else return false;
#endif
    FILE* pipe = popen(cmd, "w");
    if (!pipe) return false;
    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
    return true;
#endif
}

std::string saveToFile(const std::string& content) {
    fs::path home = getHomeDir();
    fs::path saveDir = home / "TreeVisual";
    fs::create_directories(saveDir);
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%y-%m-%d-%H-%M-%S") << "-tree.txt";
    fs::path filePath = saveDir / oss.str();
    std::ofstream ofs(filePath, std::ios::binary);
    if (ofs) {
        ofs.write(content.c_str(), content.size());
        ofs.write("\n\n", 2);
        const char* footer = R"( /$$$$$$$$                            /$$    /$$ /$$                               /$$ 
 |__  $$__/                           | $$   | $$|__/                              | $$ 
    | $$  /$$$$$$   /$$$$$$   /$$$$$$ | $$   | $$ /$$  /$$$$$$$ /$$   /$$  /$$$$$$ | $$ 
    | $$ /$$__  $$ /$$__  $$ /$$__  $$|  $$ / $$/| $$ /$$_____/| $$  | $$ |____  $$| $$ 
    | $$| $$  \__/| $$$$$$$$| $$$$$$$$ \  $$ $$/ | $$|  $$$$$$ | $$  | $$  /$$$$$$$| $$ 
    | $$| $$      | $$_____/| $$_____/  \  $$$/  | $$ \____  $$| $$  | $$ /$$__  $$| $$ 
    | $$| $$      |  $$$$$$$|  $$$$$$$   \  $/   | $$ /$$$$$$$/|  $$$$$$/|  $$$$$$$| $$ 
    |__/|__/       \_______/ \_______/    \_/    |__/|_______/  \______/  \_______/|__/ 
 Create By TreeVisual Tool
 Made by WinTerminal
 Github repo: `https://github.com/WinTerminal/TreeVisual/` 
 Bilibili: `https://space.bilibili.com/3546863863073060` 
 © 2026 WinTerminal)";
        ofs.write(footer, strlen(footer));
    }
    return filePath.string();
}

// ---------- 主函数 ----------
int main(int argc, char* argv[]) {
    bool elevated = false;
    std::vector<std::string> userArgs;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--elevated") == 0)
            elevated = true;
        else
            userArgs.push_back(argv[i]);
    }

    fs::path target;
    if (!userArgs.empty())
        target = fs::absolute(fs::path(userArgs[0]));
    else
        target = fs::current_path();

    auto lines = build_tree(target);
    size_t lineCount = lines.size();
    std::string treeText;
    for (const auto& line : lines)
        treeText += line + "\n";

    if (has_permission_error && !isAdmin() && !elevated) {
        std::cout << "\n警告：无法访问目录 \"" << first_permission_error_path.string()
                  << "\"，需要更高权限才能完整显示目录树。\n";
        std::string choice;
        while (true) {
            std::cout << "是否尝试以管理员/root 权限重新运行？(y/n): ";
            std::getline(std::cin, choice);
            if (choice == "y" || choice == "yes" || choice == "是") {
                std::vector<std::string> newArgs = userArgs;
                newArgs.push_back("--elevated");
                runAsAdmin(newArgs);
                return 0;
            } else if (choice == "n" || choice == "no" || choice == "否" || choice.empty()) {
                std::cout << "将以普通权限继续运行，无权目录将被跳过（显示[无权限访问]）。\n";
                break;
            } else {
                std::cout << "请输入 y 或 n。\n";
            }
        }
    }

    if (lineCount <= DISPLAY_LINE_THRESHOLD) {
        std::cout << treeText;
        if (copyToClipboard(treeText))
            std::cout << "\n（共 " << lineCount << " 行，已自动复制到剪贴板）\n";
    } else if (lineCount <= CLIPBOARD_LINE_THRESHOLD) {
        if (copyToClipboard(treeText))
            std::cout << "目录树共 " << lineCount << " 行，已自动复制到剪贴板（未显示）\n";
        else
            std::cout << "复制失败，以下为目录树内容：\n" << treeText;
    } else {
        std::string savedPath = saveToFile(treeText);
        std::cout << "目录树共 " << lineCount << " 行，已自动保存至：" << savedPath << "\n";
    }

    return 0;
}
