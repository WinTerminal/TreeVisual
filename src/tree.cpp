#define NOMINMAX

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

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
#include <mutex>
#include <thread>
#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <ctime>
#include <memory>
#include <functional>
#include <map>
#include <climits>
#include <clocale>
#ifndef _WIN32
#include <sys/types.h>
#include <signal.h>
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #include <windows.h>
    #include <shellapi.h>
    #include <shlobj.h>
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "advapi32.lib")
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <cstdlib>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

namespace fs = std::filesystem;

// ==================== Terminal Encoding Auto-Detection ====================
// Automatically detects and configures terminal encoding at startup.
// Prevents garbled output (mojibake) on terminals with different encodings
// (e.g., UTF-8, GBK, Big5). Falls back to ASCII-safe characters when needed.
namespace TerminalEncoding {
    enum class Mode {
        UTF8,       // Terminal supports UTF-8 output
        Fallback,   // Non-UTF-8 encoding (GBK, etc.) — use ASCII alternatives
        Unknown     // Not yet detected
    };

    Mode initialize();
    Mode currentMode();

    // Convert a UTF-8 string to the terminal's current encoding (for Windows fallback)
    std::string toTerminalEncoding(const std::string& utf8_text);

    namespace detail {
        static Mode mode_ = Mode::Unknown;
        static bool initialized_ = false;
    }
}

TerminalEncoding::Mode TerminalEncoding::initialize() {
#ifdef _WIN32
    // --- Windows: try to set console code page to UTF-8 ---
    BOOL cp_ok = SetConsoleOutputCP(CP_UTF8) && SetConsoleCP(CP_UTF8);
    if (cp_ok) {
        UINT actual_cp = GetConsoleOutputCP();
        if (actual_cp == CP_UTF8 || actual_cp == 65001) {
            detail::mode_ = Mode::UTF8;
            detail::initialized_ = true;
            return Mode::UTF8;
        }
    }

    // Fallback: keep system default (e.g., GBK 936 on Chinese Windows)
    detail::mode_ = Mode::Fallback;
    detail::initialized_ = true;
    return Mode::Fallback;

#else
    // --- Unix/Linux/macOS: use system locale ---
    char* loc = setlocale(LC_ALL, "");
    if (loc != nullptr) {
        std::string locale_str(loc);
        if (locale_str.find("UTF-8") != std::string::npos ||
            locale_str.find("utf8") != std::string::npos) {
            detail::mode_ = Mode::UTF8;
        } else if (!locale_str.empty()) {
            detail::mode_ = Mode::Fallback;   // e.g., zh_CN.GBK, zh_TW.Big5
        } else {
            detail::mode_ = Mode::Fallback;
        }
    } else {
        // setlocale("") failed — try explicit C.UTF-8
        loc = setlocale(LC_ALL, "C.UTF-8");
        if (loc) {
            detail::mode_ = Mode::UTF8;
        } else {
            setlocale(LC_ALL, "C");  // final fallback: ASCII-only
            detail::mode_ = Mode::Fallback;
        }
    }
    detail::initialized_ = true;
    return detail::mode_;
#endif
}

TerminalEncoding::Mode TerminalEncoding::currentMode() {
    if (!detail::initialized_) return initialize();
    return detail::mode_;
}

std::string TerminalEncoding::toTerminalEncoding(const std::string& utf8_text) {
    if (detail::mode_ == Mode::UTF8 ||
        detail::mode_ == Mode::Unknown ||
        utf8_text.empty()) {
        return utf8_text;
    }

#ifdef _WIN32
    // Convert UTF-8 → wide → current console code page (e.g., GBK)
    int wsize = MultiByteToWideChar(CP_UTF8, 0,
                                     utf8_text.c_str(), -1, nullptr, 0);
    if (wsize <= 0) return utf8_text;

    std::vector<wchar_t> wbuf(wsize);
    MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1,
                        wbuf.data(), wsize);

    UINT target_cp = GetConsoleOutputCP();
    int asize = WideCharToMultiByte(target_cp, 0,
                                     wbuf.data(), -1,
                                     nullptr, 0, nullptr, nullptr);
    if (asize <= 0) return utf8_text;

    std::vector<char> abuf(asize);
    WideCharToMultiByte(target_cp, 0, wbuf.data(), -1,
                        abuf.data(), asize, nullptr, nullptr);
    return std::string(abuf.data());
#else
    // On Unix, most modern systems use UTF-8 natively; no conversion needed here
    return utf8_text;
#endif
}

namespace {
    fs::path getHomeDirImpl() {
#ifdef _WIN32
        wchar_t* buf = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &buf))) {
            fs::path p(buf);
            CoTaskMemFree(buf);
            return p;
        }
        wchar_t* wprofile = _wgetenv(L"USERPROFILE");
        if (wprofile && wprofile[0] != L'\0')
            return fs::path(wprofile);
        // 兼容旧系统：尝试 SHGetFolderPath
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path)))
            return fs::path(path);
        throw std::runtime_error("Cannot determine user home directory");
#else
        const char* home = getenv("HOME");
        if (home && home[0] != '\0')
            return fs::path(home);
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0] != '\0')
            return fs::path(pw->pw_dir);
        throw std::runtime_error("Cannot determine user home directory");
#endif
    }
}

constexpr size_t DISPLAY_LINE_THRESHOLD = 50;
constexpr size_t CLIPBOARD_LINE_THRESHOLD = 100;

class PermissionErrorTracker {
public:
    static PermissionErrorTracker& instance() {
        static PermissionErrorTracker tracker;
        return tracker;
    }

    void record(const fs::path& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_error_) {
            has_error_ = true;
            first_path_ = path;
        }
    }

    bool hasError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_error_;
    }

    fs::path firstPath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return first_path_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        has_error_ = false;
        first_path_.clear();
    }

private:
    PermissionErrorTracker() : has_error_(false) {}
    ~PermissionErrorTracker() = default;

    PermissionErrorTracker(const PermissionErrorTracker&) = delete;
    PermissionErrorTracker& operator=(const PermissionErrorTracker&) = delete;

    mutable std::mutex mutex_;
    bool has_error_;
    fs::path first_path_;
};

class Node {
public:
    enum class Type { File, Directory };

    Node(const std::string& name, Type type)
        : name_(name), type_(type), children_() {}

    const std::string& name() const { return name_; }
    Type type() const { return type_; }
    const std::vector<std::shared_ptr<Node>>& children() const { return children_; }

    void addChild(std::shared_ptr<Node> child) {
        children_.push_back(std::move(child));
    }

    bool isDirectory() const { return type_ == Type::Directory; }
    bool isFile() const { return type_ == Type::File; }

private:
    std::string name_;
    Type type_;
    std::vector<std::shared_ptr<Node>> children_;
};

class ITraversalPolicy {
public:
    virtual ~ITraversalPolicy() = default;
    virtual std::vector<fs::directory_entry> listDirectory(const fs::path& dir, bool show_hidden) = 0;
    virtual bool isDirectory(const fs::directory_entry& entry) = 0;
};

class DefaultTraversalPolicy : public ITraversalPolicy {
public:
    std::vector<fs::directory_entry> listDirectory(const fs::path& dir, bool show_hidden) override {
        std::error_code ec;
        fs::directory_iterator it(dir, ec);
        if (ec) {
            PermissionErrorTracker::instance().record(dir);
            return {};
        }

        std::vector<fs::directory_entry> entries;
        for (auto& entry : it) {
            const auto& name = entry.path().filename().string();
            if (!show_hidden && !name.empty() && name[0] == '.')
                continue;
            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                std::string an = a.path().filename().string();
                std::string bn = b.path().filename().string();
                std::transform(an.begin(), an.end(), an.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });
                std::transform(bn.begin(), bn.end(), bn.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });
                return an < bn;
            });

        return entries;
    }

    bool isDirectory(const fs::directory_entry& entry) override {
        std::error_code ec;
        auto st = entry.symlink_status(ec);
        if (ec) return false;
        if (fs::is_symlink(st)) return false;
        return fs::is_directory(st);
    }
};

class DirectoryTree {
public:
    DirectoryTree(std::unique_ptr<ITraversalPolicy> policy = std::make_unique<DefaultTraversalPolicy>())
        : root_(nullptr), policy_(std::move(policy)), root_path_() {}

    virtual ~DirectoryTree() = default;

    virtual std::shared_ptr<Node> build(const fs::path& root, bool show_hidden) {
        PermissionErrorTracker::instance().reset();
        root_path_ = root;
        root_ = buildNode(root, show_hidden, 0);
        return root_;
    }

    const fs::path& rootPath() const { return root_path_; }
    void setRootPath(const fs::path& path) { root_path_ = path; }

protected:
    static constexpr size_t MAX_DEPTH = 512;

    std::shared_ptr<Node> root_;
    fs::path root_path_;

    virtual std::shared_ptr<Node> buildNode(const fs::path& path, bool show_hidden, size_t depth = 0) {
        if (depth > MAX_DEPTH) {
            std::cerr << "Warning: maximum recursion depth (" << MAX_DEPTH << ") exceeded, skipping " << path.string() << "\n";
            return nullptr;
        }
        std::string name = path.filename().string();
        if (name.empty()) name = path.string();

        auto node = std::make_shared<Node>(name, Node::Type::Directory);

        auto entries = policy_->listDirectory(path, show_hidden);
        if (entries.empty()) {
            return node;
        }

        std::vector<fs::directory_entry> subdirs, files;
        for (const auto& entry : entries) {
            if (policy_->isDirectory(entry))
                subdirs.push_back(entry);
            else
                files.push_back(entry);
        }

        for (const auto& entry : files) {
            auto file = std::make_shared<Node>(entry.path().filename().string(), Node::Type::File);
            node->addChild(file);
        }

        for (const auto& entry : subdirs) {
            auto dir = buildNode(entry.path(), show_hidden, depth + 1);
            if (dir) node->addChild(dir);
        }

        return node;
    }

    std::unique_ptr<ITraversalPolicy> policy_;
};

class ParallelDirectoryTree : public DirectoryTree {
public:
    ParallelDirectoryTree(unsigned int max_threads = 0)
        : DirectoryTree(std::make_unique<DefaultTraversalPolicy>())
        , max_threads_(max_threads) {}

    std::shared_ptr<Node> build(const fs::path& root, bool show_hidden) override {
        PermissionErrorTracker::instance().reset();

        auto entries = policy_->listDirectory(root, show_hidden);

        std::string root_name = root.filename().string();
        if (root_name.empty()) root_name = root.string();
        root_ = std::make_shared<Node>(root_name, Node::Type::Directory);
        root_path_ = root;

        if (entries.empty()) {
            return root_;
        }

        std::vector<fs::directory_entry> subdirs, files;
        for (const auto& entry : entries) {
            if (policy_->isDirectory(entry))
                subdirs.push_back(entry);
            else
                files.push_back(entry);
        }

        for (const auto& entry : files) {
            auto file = std::make_shared<Node>(entry.path().filename().string(), Node::Type::File);
            root_->addChild(file);
        }

        if (subdirs.empty()) {
            return root_;
        }

        unsigned int num_threads = (max_threads_ > 0) ? max_threads_ : std::max(1u, std::thread::hardware_concurrency());
        if (num_threads > static_cast<unsigned int>(subdirs.size())) {
            num_threads = static_cast<unsigned int>(subdirs.size());
        }

        std::vector<std::shared_ptr<Node>> sub_results(subdirs.size());
        std::atomic<size_t> next_idx(0);
        std::vector<std::thread> workers;

        for (unsigned int w = 0; w < num_threads; ++w) {
            workers.emplace_back([&, show_hidden]() {
                while (true) {
                    size_t idx = next_idx.fetch_add(1);
                    if (idx >= subdirs.size()) break;
                    sub_results[idx] = buildSubTree(subdirs[idx].path(), show_hidden, *policy_);
                }
            });
        }
        for (auto& worker : workers) worker.join();

        for (size_t i = 0; i < subdirs.size(); ++i) {
            auto& sub = subdirs[i];
            auto sub_node = sub_results[i];
            auto named_node = std::make_shared<Node>(
                sub.path().filename().string(), Node::Type::Directory);
            for (const auto& child : sub_node->children()) {
                named_node->addChild(child);
            }
            root_->addChild(named_node);
        }

        return root_;
    }

private:
    std::shared_ptr<Node> buildSubTree(const fs::path& path, bool show_hidden, ITraversalPolicy& policy) {
        std::string name = path.filename().string();
        if (name.empty()) name = path.string();

        auto node = std::make_shared<Node>(name, Node::Type::Directory);

        auto entries = policy.listDirectory(path, show_hidden);
        if (entries.empty()) {
            return node;
        }

        std::vector<fs::directory_entry> subdirs, files;
        for (const auto& entry : entries) {
            if (policy.isDirectory(entry))
                subdirs.push_back(entry);
            else
                files.push_back(entry);
        }

        for (const auto& entry : files) {
            auto file = std::make_shared<Node>(entry.path().filename().string(), Node::Type::File);
            node->addChild(file);
        }

        for (const auto& entry : subdirs) {
            auto sub = buildSubTree(entry.path(), show_hidden, policy);
            if (sub) node->addChild(sub);
        }

        return node;
    }

    unsigned int max_threads_;
};

class IOutputHandler {
public:
    virtual ~IOutputHandler() = default;
    virtual bool handle(const std::string& content, size_t line_count) = 0;
};

class ConsoleOutputHandler : public IOutputHandler {
public:
    bool handle(const std::string& content, size_t line_count) override {
        std::cout << content;
        return true;
    }
};

class ClipboardOutputHandler : public IOutputHandler {
public:
    bool handle(const std::string& content, size_t line_count) override {
        return copyToClipboard(content);
    }

private:
    const char* cached_cmd_ = nullptr;
    bool cmd_checked_ = false;

    const char* detectClipboardCommand() const {
#ifdef _WIN32
        return nullptr;
#elif defined(__APPLE__)
        return "pbcopy";
#else
        if (system("which xclip >/dev/null 2>&1") == 0)
            return "xclip -selection clipboard";
        else if (system("which xsel >/dev/null 2>&1") == 0)
            return "xsel -i -b";
        else
            return nullptr;
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
        wchar_t* wstr = static_cast<wchar_t*>(GlobalLock(hMem));
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wstr, utf16size);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
        CloseClipboard();
        return true;
#else
        if (!cmd_checked_) {
            cached_cmd_ = detectClipboardCommand();
            cmd_checked_ = true;
        }
        if (!cached_cmd_) return false;
        FILE* pipe = popen(cached_cmd_, "w");
        if (!pipe) return false;
        fwrite(text.c_str(), 1, text.size(), pipe);
        pclose(pipe);
        return true;
#endif
    }
};

class FileOutputHandler : public IOutputHandler {
public:
    bool handle(const std::string& content, size_t line_count) override {
        try {
            fs::path path = saveToFile(content);
            std::cout << "Tree contains " << line_count << " lines, automatically saved to: " << path.string() << "\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Save failed: " << e.what() << std::endl;
            return false;
        }
    }

private:
    fs::path getHomeDir() {
        return getHomeDirImpl();
    }

    fs::path saveToFile(const std::string& content) {
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
        oss << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S") << "-tree.txt";
        fs::path filePath = saveDir / oss.str();
        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs)
            throw std::runtime_error("Cannot write to file: " + filePath.string());
        ofs.write(content.c_str(), content.size());
        ofs.write("\n\n", 2);
        const char* footer =
            " /$$$$$$$$                            /$$    /$$ /$$                               /$$\n"
            " |__  $$__/                           | $$   | $$|__/                              | $$\n"
            "    | $$  /$$$$$$   /$$$$$$   /$$$$$$ | $$   | $$ /$$  /$$$$$$$ /$$   /$$  /$$$$$$ | $$\n"
            "    | $$ /$$__  $$ /$$__  $$ /$$__  $$|  $$ / $$/| $$ /$$_____/| $$  | $$ |____  $$| $$\n"
            "    | $$| $$  \\__/| $$$$$$$$| $$$$$$$$ \\  $$ $$/ | $$|  $$$$$$ | $$  | $$  /$$$$$$$| $$\n"
            "    | $$| $$      | $$_____/| $$_____/  \\  $$$/  | $$ \\____  $$| $$  | $$ /$$__  $$| $$\n"
            "    | $$| $$      |  $$$$$$$|  $$$$$$$   \\  $/   | $$ /$$$$$$$/|  $$$$$$/|  $$$$$$$| $$\n"
            "    |__/|__/       \\_______/ \\_______/    \\_/    |__/|_______/  \\______/  \\_______/|__/\n"
            " Created By TreeVisual Tool\n"
            " Made by WinTerminal\n"
            " Github repo: `https://github.com/WinTerminal/TreeVisual/`\n"
            " Bilibili: `https://space.bilibili.com/3546863863073060`\n"
             " © 2026 WinTerminal\n";
ofs.write(footer, strlen(footer));
        return filePath;
    }
};

class IPlatformHelper {
public:
    virtual ~IPlatformHelper() = default;
    virtual fs::path getHomeDir() = 0;
    virtual bool isAdmin() = 0;
    virtual bool runAsAdmin(const std::vector<std::string>& args) = 0;
};

#ifdef _WIN32

class WindowsPlatformHelper : public IPlatformHelper {
public:
    fs::path getHomeDir() override {
        return getHomeDirImpl();
    }

    bool isAdmin() override {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &adminGroup)) {
            if (adminGroup) {
                if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin))
                    isAdmin = FALSE;
                FreeSid(adminGroup);
            }
        }
        return isAdmin != FALSE;
    }

    bool runAsAdmin(const std::vector<std::string>& args) override {
        std::cout << R"(/$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$         /$$$$$$  /$$$$$$$  /$$      /$$ /$$$$$$ /$$   /$$
 /$$__  $$| $$  | $$|__  $$__//$$__  $$       /$$__  $$| $$__  $$| $$$    /$$$|_  $$_/| $$$ | $$
| $$  \ $$| $$  | $$   | $$  | $$  \ $$      | $$  \ $$| $$  $$| $$$$  /$$$$  | $$  | $$$$| $$
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
            int size = MultiByteToWideChar(CP_UTF8, 0, a.c_str(), -1, nullptr, 0);
            std::wstring wArg(size - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, a.c_str(), -1, &wArg[0], size);
            std::wstring escaped;
            for (wchar_t c : wArg) {
                if (c == L'\\') escaped += L"\\\\";
                else if (c == L'"') escaped += L"\\\"";
                else escaped += c;
            }
            cmdLine += L"\"" + escaped + L"\"";
        }
        HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath, cmdLine.c_str(), nullptr, SW_NORMAL);
        return reinterpret_cast<intptr_t>(result) > 32;
    }
};

#else

class UnixPlatformHelper : public IPlatformHelper {
public:
    fs::path getHomeDir() override {
        return getHomeDirImpl();
    }

    bool isAdmin() override {
        return geteuid() == 0;
    }

    bool runAsAdmin(const std::vector<std::string>& args) override {
        std::cout << R"(/$$$$$$  /$$   /$$ /$$$$$$$$ /$$$$$$         /$$$$$$  /$$   /$$ /$$$$$$$   /$$$$$$ 
 /$$__  $$| $$  | $$|__  $$__//$$__  $$       /$$__  $$| $$  | $$| $$__  $$ /$$__  $$
| $$  \ $$| $$  | $$   | $$  | $$  \ $$      | $$  \__/| $$  | $$| $$  \ $$| $$  \ $$
| $$$$$$$$| $$  | $$   | $$  | $$  | $$      |  $$$$$$ | $$  | $$| $$  | $$| $$  | $$
| $$__  $$| $$  | $$   | $$  | $$  | $$       \____  $$| $$  | $$| $$  | $$| $$  | $$
| $$  | $$| $$  | $$   | $$  | $$  | $$       /$$  \ $$| $$  | $$| $$  | $$| $$  | $$
| $$  | $$|  $$$$$$/   | $$  |  $$$$$$/      |  $$$$$$/|  $$$$$$/| $$$$$$$/|  $$$$$$/
|__/  |__/ \______/    |__/   \______/        \______/  \______/ |_______/  \______/ 
)" << std::endl;
        std::string exePath;
        try { exePath = fs::canonical("/proc/self/exe").string(); }
        catch (...) { return false; }
        std::vector<const char*> argv;
        argv.push_back("sudo");
        argv.push_back(exePath.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp("sudo", const_cast<char* const*>(argv.data()));
        perror("execvp sudo");
        return false;
    }
};

#endif

class TreeFormatter {
public:
    static std::string format(const std::shared_ptr<Node>& root, bool show_hidden) {
        std::vector<std::string> lines;
        formatNode(root, "", true, lines);
        std::string result;
        for (const auto& line : lines)
            result += line + "\n";
        return result;
    }
    
    static std::vector<std::string> formatLines(const std::shared_ptr<Node>& root, int limit = 0) {
        std::vector<std::string> lines;
        formatNode(root, "", true, lines);
        if (limit > 0 && lines.size() > limit) {
            lines.resize(limit);
            lines.push_back("... (" + std::to_string(limit) + " lines)");
        }
        return lines;
    }

private:
    static void formatNode(const std::shared_ptr<Node>& node, const std::string& prefix,
                         bool is_last, std::vector<std::string>& lines) {
        // Encoding-aware tree drawing: use ASCII-safe characters on non-UTF-8 terminals
        const bool use_ascii = (TerminalEncoding::currentMode() != TerminalEncoding::Mode::UTF8);

        std::string name = node->name();
        if (node->isDirectory() && name.find('/') == std::string::npos)
            name += "/";

        if (!prefix.empty()) {
            const char* conn_last  = use_ascii ? "+-- " : "\xe2\x94\x94\xe2\x94\x80 ";
            const char* conn_mid   = use_ascii ? "|-- " : "\xe2\x94\x9c\xe2\x94\x80 ";
            const char* pref_last  = use_ascii ? "    " : "    ";
            const char* pref_mid   = use_ascii ? "|   " : "\xe2\x94\x82   ";

            std::string connector = is_last ? conn_last : conn_mid;
            lines.push_back(prefix + connector + name);
        } else {
            lines.push_back(name);
        }

        const auto& children = node->children();
        if (children.empty()) return;

        size_t count = children.size();
        for (size_t i = 0; i < count; ++i) {
            bool child_is_last = (i == count - 1);
            const char* pref = (is_last
                ? (use_ascii ? "    " : "    ")
                : (use_ascii ? "|   " : "\xe2\x94\x82   "));
            std::string child_prefix = prefix + pref;
            formatNode(children[i], child_prefix, child_is_last, lines);
        }
    }
};

// ==================== JSON Utilities ====================

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static std::string nodeToJson(const std::shared_ptr<Node>& node, const std::string& absPath = "", bool shallow = false) {
    std::string type = node->isDirectory() ? "directory" : "file";
    std::string json = "{\"name\":\"" + jsonEscape(node->name()) + "\",\"type\":\"" + type + "\"";
    if (node->isDirectory()) {
        json += ",\"path\":\"" + jsonEscape(absPath) + "\"";
        bool hasChildren = !node->children().empty();
        json += ",\"hasChildren\":" + std::string(hasChildren ? "true" : "false");
        if (!shallow && hasChildren) {
            json += ",\"children\":[";
            const auto& children = node->children();
            for (size_t i = 0; i < children.size(); ++i) {
                if (i > 0) json += ",";
                std::string childPath;
                if (!absPath.empty()) {
                    childPath = (fs::path(absPath) / children[i]->name()).string();
                }
                json += nodeToJson(children[i], childPath, false);
            }
            json += "]";
        }
    }
    json += "}";
    return json;
}

// Shallow version: only 1 level, directories get hasChildren flag
static std::string dirTreeToJson(const fs::path& dir, bool show_hidden) {
    DefaultTraversalPolicy policy;
    auto entries = policy.listDirectory(dir, show_hidden);
    std::vector<fs::directory_entry> subdirs, files;
    for (const auto& entry : entries) {
        if (policy.isDirectory(entry))
            subdirs.push_back(entry);
        else
            files.push_back(entry);
    }
    std::string json = "{\"name\":\"";
    std::string dirName = dir.filename().string();
    if (dirName.empty()) dirName = dir.string();
    json += jsonEscape(dirName) + "\",\"type\":\"directory\",\"path\":\"" + jsonEscape(dir.string()) + "\",\"children\":[";
    bool first = true;
    for (const auto& entry : files) {
        if (!first) json += ",";
        json += "{\"name\":\"" + jsonEscape(entry.path().filename().string()) + "\",\"type\":\"file\"}";
        first = false;
    }
    for (const auto& entry : subdirs) {
        if (!first) json += ",";
        fs::path childPath = entry.path();
        std::error_code ec;
        auto st = entry.symlink_status(ec);
        bool isSymlink = !ec && fs::is_symlink(st);
        json += "{\"name\":\"" + jsonEscape(entry.path().filename().string()) + "\",\"type\":\"directory\",\"path\":\"" + jsonEscape(childPath.string()) + "\"";
        if (!isSymlink) {
            auto subEntries = policy.listDirectory(childPath, show_hidden);
            json += ",\"hasChildren\":" + std::string(!subEntries.empty() ? "true" : "false");
        } else {
            json += ",\"hasChildren\":false";
        }
        json += "}";
        first = false;
    }
    json += "]}";
    return json;
}

static std::string dirListToJson(const fs::path& dir, bool show_hidden) {
    DefaultTraversalPolicy policy;
    auto entries = policy.listDirectory(dir, show_hidden);
    std::string json = "[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) json += ",";
        const auto& e = entries[i];
        std::string name = e.path().filename().string();
        bool is_dir = policy.isDirectory(e);
        json += "{\"name\":\"" + jsonEscape(name) + "\",\"isDir\":" + (is_dir ? "true" : "false") + "}";
    }
    json += "]";
    return json;
}

// ==================== ServiceManager ====================
// Daemon process management and settings persistence

// Forward declaration
class WebServer;

class ServiceManager {
    // Cross-platform PID type
#ifdef _WIN32
    using PidType = unsigned long;   // DWORD on Windows
#else
    using PidType = pid_t;
#endif

public:
    static std::string configDir() {
        std::string dir;
#ifdef _WIN32
        char* appdata = getenv("LOCALAPPDATA");
        dir = appdata ? std::string(appdata) + "\\TreeVisual" : ".\\TreeVisual";
#else
        char* home = getenv("HOME");
        dir = home ? std::string(home) + "/.config/treevisual" : "/tmp/.treevisual";
#endif
        return dir;
    }

    static std::string pidFilePath() { return configDir() + "/tree.pid"; }
    static std::string settingsFilePath() { return configDir() + "/settings.json"; }

    // Check if service is currently running (PID file exists + process alive)
    static bool isRunning() {
        auto pid = readPid();
        if (pid <= 0) return false;
#ifdef _WIN32
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
        if (h) { CloseHandle(h); return true; }
        return false;
#else
        return (kill((pid_t)pid, 0) == 0);
#endif
    }

    // Start daemon: fork into background, write PID, start WebServer
    static bool start(int port = 7200);

    // Stop daemon: kill process by PID, clean up
    static bool stop() {
        auto pid = readPid();
        if (pid <= 0) {
            std::cout << "[Service] No running instance found\n";
            return false;
        }
        std::cout << "[Service] Stopping PID " << pid << "...";
#ifdef _WIN32
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
        if (h) { TerminateProcess(h, 0); CloseHandle(h); }
#else
        kill((pid_t)pid, SIGTERM);
        // Wait up to 5 seconds for graceful shutdown
        for (int i = 0; i < 50; ++i) {
            if (kill((pid_t)pid, 0) != 0) break;
            usleep(100000);
        }
        // Force kill if still alive
        if (kill((pid_t)pid, 0) == 0)
            kill((pid_t)pid, SIGKILL);
#endif
        removePid();
        std::cout << " stopped.\n";
        return true;
    }

    // Restart: stop then start
    static bool restart(int port = 7200) {
        stop();
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
        return start(port);
    }

    // Settings persistence: load from JSON file
    static std::string loadSettings() {
        std::string path = settingsFilePath();
        std::ifstream ifs(path);
        if (!ifs.is_open()) return "{}";
        std::string content((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
        ifs.close();
        if (content.empty()) return "{}";
        return content;
    }

    // Settings persistence: save to JSON file
    static bool saveSettings(const std::string& json) {
        fs::create_directories(configDir());
        std::string path = settingsFilePath();
        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << json;
        ofs.close();
        return true;
    }

private:
    static PidType readPid() {
        std::ifstream ifs(pidFilePath());
        if (!ifs.is_open()) return (PidType)-1;
        PidType pid;
        ifs >> pid;
        ifs.close();
        return pid;
    }

    static void writePid(PidType pid) {
        std::ofstream ofs(pidFilePath());
        ofs << pid << "\n";
        ofs.close();
    }

    static void removePid() {
        std::remove(pidFilePath().c_str());
    }

#ifdef _WIN32
    static std::string getExecutablePath() {
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
        return len > 0 ? std::string(buf, len) : "tree.exe";
    }
#endif
};

// ==================== WebServer ====================

class WebServer {
public:
    WebServer(int port = 7200) : port_(port), running_(false) {}

    void start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return;
        }
#endif

        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            std::cerr << "Failed to create socket\n";
            return;
        }

        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<const char*>(&opt), sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "Failed to bind port " << port_ << "\n";
            closeSocket(server_fd_);
            return;
        }

        if (listen(server_fd_, 16) < 0) {
            std::cerr << "Failed to listen\n";
            closeSocket(server_fd_);
            return;
        }

        running_ = true;
        std::cout << "TreeVisual WebUI running on http://0.0.0.0:" << port_ << "/\n";
        std::cout << "  http://127.0.0.1:" << port_ << "/\n";
        std::cout << "Press Ctrl+C to stop.\n";

        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_,
                reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) {
                if (running_) std::cerr << "Accept failed\n";
                continue;
            }
            std::thread(&WebServer::handleClient, this, client_fd).detach();
        }

        closeSocket(server_fd_);

#ifdef _WIN32
        WSACleanup();
#endif
    }

    void stop() {
        running_ = false;
        closeSocket(server_fd_);
    }

private:
    int port_;
    std::atomic<bool> running_;
    int server_fd_;

    static void closeSocket(int fd) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
    }

    // Get the directory containing the executable (platform-specific)
    static fs::path getExecutableDirectory() {
#ifdef _WIN32
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
        if (len > 0 && len < sizeof(buf)) {
            return fs::path(std::string(buf, len)).parent_path();
        }
#else
        char buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            return fs::path(std::string(buf)).parent_path();
        }
#endif
        return fs::current_path();
    }

    static std::string recvRequest(int fd) {
        char buf[4096];
        std::string req;
        while (true) {
            int n = recv(fd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            req += buf;
            if (req.find("\r\n\r\n") != std::string::npos) break;
        }
        return req;
    }

    static void sendResponse(int fd, int status, const std::string& content_type,
                             const std::string& body) {
        std::string status_text = (status == 200) ? "OK" :
                                   (status == 404) ? "Not Found" : "Bad Request";
        std::ostringstream resp;
        resp << "HTTP/1.1 " << status << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;
        std::string data = resp.str();
        const char* ptr = data.data();
        size_t remaining = data.size();
        while (remaining > 0) {
            int n = send(fd, ptr, remaining, 0);
            if (n <= 0) break;
            ptr += n;
            remaining -= static_cast<size_t>(n);
        }
    }

    void handleClient(int client_fd) {
        std::string req = recvRequest(client_fd);
        if (req.empty()) {
            closeSocket(client_fd);
            return;
        }

        size_t method_end = req.find(' ');
        size_t path_end = req.find(' ', method_end + 1);
        if (method_end == std::string::npos || path_end == std::string::npos) {
            sendResponse(client_fd, 400, "text/plain", "Bad Request");
            closeSocket(client_fd);
            return;
        }

        std::string method = req.substr(0, method_end);
        std::string full_path = req.substr(method_end + 1, path_end - method_end - 1);

        std::string path = full_path;
        std::string query;

        // Allow POST only for API endpoints that need it
        if (method == "POST" && path != "/api/settings"
                           && path != "/api/service/start"
                           && path != "/api/service/stop") {
            sendResponse(client_fd, 400, "text/plain", "Method Not Allowed");
            closeSocket(client_fd);
            return;
        } else if (method != "GET" && method != "POST") {
            sendResponse(client_fd, 400, "text/plain", "Method Not Allowed");
            closeSocket(client_fd);
            return;
        }

        size_t qpos = path.find('?');
        if (qpos != std::string::npos) {
            query = path.substr(qpos + 1);
            path = path.substr(0, qpos);
        }

        // Route: static files or API or fallback
        if (path == "/" || path == "/index.html") {
            serveStaticOrFallback(client_fd, "index.html", "text/html; charset=utf-8");
        } else if (path == "/styles.css") {
            serveStaticOrFallback(client_fd, "styles.css", "text/css; charset=utf-8");
        } else if (path == "/app.js") {
            serveStaticOrFallback(client_fd, "app.js", "application/javascript; charset=utf-8");
        } else if (path == "/i18n.js") {
            serveStaticOrFallback(client_fd, "i18n.js", "application/javascript; charset=utf-8");
        } else if (path == "/webgl.js") {
            serveStaticOrFallback(client_fd, "webgl.js", "application/javascript; charset=utf-8");
        } else if (path == "/api/settings") {
            handleApiSettings(client_fd, method, req);  // Allow GET + POST
        } else if (path == "/api/service/status") {
            handleApiServiceStatus(client_fd);
        } else if (path == "/api/service/start") {
            handleApiServiceStart(client_fd);
        } else if (path == "/api/service/stop") {
            handleApiServiceStop(client_fd);
        } else if (path == "/api/tree") {
            handleApiTree(client_fd, query);
        } else if (path == "/api/list") {
            handleApiList(client_fd, query);
        } else if (path == "/api/home") {
            handleApiHome(client_fd);
        } else {
            sendResponse(client_fd, 404, "text/plain", "Not Found");
        }

        closeSocket(client_fd);
    }

    // ===== Static File Serving =====

    static fs::path getWebRoot() {
#ifdef WEB_ROOT_PATH
        fs::path compileTimePath(WEB_ROOT_PATH);
        if (fs::exists(compileTimePath) && fs::is_directory(compileTimePath))
            return compileTimePath;
#endif

        // Try relative to executable
        std::error_code ec;
        fs::path exeDir = getExecutableDirectory();
        fs::path candidates[] = {
            exeDir / "web",
            exeDir / ".." / "web",
            exeDir / ".." / ".." / "src" / "web"
        };

        for (auto& p : candidates) {
            if (fs::exists(p, ec) && fs::is_directory(p, ec)) return p;
        }
        return "";  // empty = not found, will use fallback
    }

    static std::string getMimeType(const std::string& ext) {
        static const std::map<std::string, std::string> mimeMap = {
            {".html", "text/html; charset=utf-8"},
            {".htm",  "text/html; charset=utf-8"},
            {".css",  "text/css; charset=utf-8"},
            {".js",   "application/javascript; charset=utf-8"},
            {".json", "application/json"},
            {".png",  "image/png"},
            {".svg",  "image/svg+xml"}
        };
        auto it = mimeMap.find(ext);
        return (it != mimeMap.end()) ? it->second : "application/octet-stream";
    }

    static void serveStaticOrFallback(int client_fd, const std::string& filename,
                                        const std::string& defaultMime) {
        auto webRoot = getWebRoot();
        if (!webRoot.empty()) {
            fs::path filePath = webRoot / filename;
            std::error_code ec;
            if (fs::exists(filePath, ec) && fs::is_regular_file(filePath, ec) && !fs::is_symlink(filePath, ec)) {
                // Security: ensure file is within webroot
                std::error_code ec2;
                auto canonicalFile = fs::canonical(filePath, ec2);
                if (ec2) { sendResponse(client_fd, 500, "text/plain", "Internal Server Error"); return; }
                auto canonicalRoot = fs::canonical(webRoot, ec2);
                if (ec2) { sendResponse(client_fd, 500, "text/plain", "Internal Server Error"); return; }

                std::string fStr = canonicalFile.string();
                std::string rStr = canonicalRoot.string();

                if (fStr.compare(0, rStr.size(), rStr) == 0) {
                    serveStaticFile(client_fd, filePath);
                    return;
                }
            }
        }
        // Fallback: return embedded HTML for index.html, 404 for others
        if (filename == "index.html") {
            sendResponse(client_fd, 200, defaultMime, getWebUIHtml());
        } else {
            sendResponse(client_fd, 404, "text/plain", "Not Found");
        }
    }

    static void serveStaticFile(int client_fd, const fs::path& filePath) {
        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs.is_open()) {
            sendResponse(client_fd, 500, "text/plain", "Internal Server Error");
            return;
        }

        std::string body((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
        ifs.close();

        // Limit response size (10MB)
        if (body.size() > 10 * 1024 * 1024) {
            sendResponse(client_fd, 413, "text/plain", "Payload Too Large");
            return;
        }

        std::string ext = filePath.extension().string();
        sendResponse(client_fd, 200, getMimeType(ext), body);
    }

    static std::string getQueryParam(const std::string& query, const std::string& key) {
        std::string prefix = key + "=";
        size_t pos = query.find(prefix);
        if (pos == std::string::npos) return "";
        size_t start = pos + prefix.size();
        size_t end = query.find('&', start);
        std::string val = (end == std::string::npos) ? query.substr(start) : query.substr(start, end - start);
        std::string decoded;
        for (size_t i = 0; i < val.size(); ++i) {
            if (val[i] == '%' && i + 2 < val.size()) {
                char hex[3] = {val[i+1], val[i+2], 0};
                decoded += static_cast<char>(strtol(hex, nullptr, 16));
                i += 2;
            } else if (val[i] == '+') {
                decoded += ' ';
            } else {
                decoded += val[i];
            }
        }
        return decoded;
    }

    void handleApiTree(int fd, const std::string& query) {
        std::string pathStr = getQueryParam(query, "path");
        if (pathStr.empty()) pathStr = fs::current_path().string();
        bool showHidden = (getQueryParam(query, "show_hidden") == "true");

        fs::path target(pathStr);
        std::error_code ec;
        if (!fs::exists(target, ec) || !fs::is_directory(target, ec)) {
            sendResponse(fd, 200, "application/json",
                "{\"error\":\"Invalid path: " + jsonEscape(pathStr) + "\"}");
            return;
        }

        std::string json = dirTreeToJson(target, showHidden);
        sendResponse(fd, 200, "application/json", json);
    }

    void handleApiList(int fd, const std::string& query) {
        std::string pathStr = getQueryParam(query, "path");
        if (pathStr.empty()) pathStr = fs::current_path().string();
        bool showHidden = (getQueryParam(query, "show_hidden") == "true");

        fs::path target(pathStr);
        std::error_code ec;
        if (!fs::exists(target, ec) || !fs::is_directory(target, ec)) {
            sendResponse(fd, 200, "application/json",
                "{\"error\":\"Invalid path: " + jsonEscape(pathStr) + "\"}");
            return;
        }

        std::string json = dirListToJson(target, showHidden);
        sendResponse(fd, 200, "application/json", json);
    }

    void handleApiHome(int fd) {
        std::string home;
#ifdef _WIN32
        char* userprofile = getenv("USERPROFILE");
        home = userprofile ? std::string(userprofile) : "C:\\";
#else
        char* homeEnv = getenv("HOME");
        home = homeEnv ? std::string(homeEnv) : "/";
#endif
        sendResponse(fd, 200, "application/json",
            "{\"path\":\"" + jsonEscape(home) + "\"}");
    }

    // ===== Settings API =====
    void handleApiSettings(int fd, const std::string& method, const std::string& rawReq) {
        if (method == "GET") {
            std::string settings = ServiceManager::loadSettings();
            sendResponse(fd, 200, "application/json", settings);
        } else if (method == "POST") {
            // Extract body from request (after \r\n\r\n)
            size_t bodyStart = rawReq.find("\r\n\r\n");
            std::string body = (bodyStart != std::string::npos)
                ? rawReq.substr(bodyStart + 4) : "";
            // URL-decode for safety
            ServiceManager::saveSettings(body);
            sendResponse(fd, 200, "application/json",
                "{\"success\":true,\"message\":\"Settings saved\"}");
        } else {
            sendResponse(fd, 405, "text/plain", "Method Not Allowed");
        }
    }

    // ===== Service API =====
    void handleApiServiceStatus(int fd) {
        bool running = ServiceManager::isRunning();
        std::string pidStr;
        auto pidFile = ServiceManager::pidFilePath();
        std::ifstream ifs(pidFile);
        if (ifs.is_open()) { ifs >> pidStr; ifs.close(); }
        std::ostringstream jss;
        jss << "{\"running\":" << (running ? "true" : "false")
            << ",\"pid\":\"" << (pidStr.empty() ? "0" : pidStr) << "\"}";
        sendResponse(fd, 200, "application/json", jss.str());
    }

    void handleApiServiceStart(int fd) {
        bool ok = ServiceManager::start(7200);
        std::string json = "{\"success\":" + std::string(ok ? "true" : "false")
                         + ",\"message\":\"" + (ok ? "Started" : "Failed to start") + "\"}";
        sendResponse(fd, 200, "application/json", json);
    }

    void handleApiServiceStop(int fd) {
        bool ok = ServiceManager::stop();
        std::string json = "{\"success\":" + std::string(ok ? "true" : "false")
                         + ",\"message\":\"" + (ok ? "Stopped" : "No instance running") + "\"}";
        sendResponse(fd, 200, "application/json", json);
    }

    static std::string getWebUIHtml() {
        return R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TreeVisual WebUI</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1b26;color:#c0caf5;font-family:"Cascadia Code","Fira Code","JetBrains Mono",monospace;height:100vh;display:flex;flex-direction:column}
.header{background:#24283b;padding:12px 20px;display:flex;align-items:center;gap:12px;border-bottom:1px solid #3b4261}
.header h1{color:#7aa2f7;font-size:18px;white-space:nowrap}
.header span{color:#565f89;font-size:12px}
#pathInput{flex:1;background:#1a1b26;border:1px solid #3b4261;color:#c0caf5;padding:8px 12px;border-radius:6px;font-family:inherit;font-size:14px;outline:none}
#pathInput:focus{border-color:#7aa2f7}
button{background:#7aa2f7;color:#1a1b26;border:none;padding:8px 20px;border-radius:6px;cursor:pointer;font-family:inherit;font-weight:600;font-size:14px}
button:hover{background:#bb9af7}
.toolbar{background:#24283b;padding:6px 20px;display:flex;gap:8px;align-items:center;border-bottom:1px solid #3b4261}
.toolbar button{padding:4px 12px;font-size:12px}
#status{color:#565f89;font-size:12px;margin-left:auto}
.settings-btn{position:relative}
.settings-panel{display:none;position:absolute;top:100%;right:0;background:#1a1b26;border:1px solid #3b4261;border-radius:6px;padding:16px;min-width:240px;z-index:100;box-shadow:0 8px 32px rgba(0,0,0,0.4)}
.settings-panel.open{display:block}
.settings-panel h3{color:#7aa2f7;font-size:14px;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #3b4261}
.setting-item{display:flex;align-items:center;justify-content:space-between;padding:8px 0}
.setting-item label{color:#c0caf5;font-size:13px;cursor:pointer}
.setting-item input[type=checkbox]{width:16px;height:16px;accent-color:#7aa2f7;cursor:pointer}
.content{flex:1;overflow:auto;padding:20px}
.tree-line{white-space:pre;min-height:1.4em;line-height:1.4}
.tree-line:hover{background:#292e42}
.dir-link{color:#7aa2f7;cursor:pointer;text-decoration:none}
.dir-link:hover{text-decoration:underline}
.file-name{color:#9ece6a}
.loading{color:#e0af68;text-align:center;padding:40px;font-size:16px}
.error{color:#f7768e;padding:20px}
</style>
</head>
<body>
<div class="header">
  <h1>TreeVisual</h1>
  <span>v1.1.2</span>
  <input type="text" id="pathInput" placeholder="Enter directory path...">
  <button onclick="scanDirectory()">Scan</button>
</div>
<div class="toolbar">
  <button onclick="goUp()">Up</button>
  <button onclick="goHome()">Home</button>
  <button onclick="refresh()">Refresh</button>
  <div class="settings-btn">
    <button onclick="toggleSettings()">&#9881; Settings</button>
    <div class="settings-panel" id="settingsPanel">
      <h3>Settings</h3>
      <div class="setting-item">
        <label for="showHidden">Show hidden files</label>
        <input type="checkbox" id="showHidden">
      </div>
    </div>
  </div>
  <span id="status"></span>
</div>
<div class="content" id="treeContainer">
  <div class="loading">Enter a path and click Scan to visualize a directory tree.</div>
</div>
<script>
var currentPath = "";
var container = document.getElementById("treeContainer");
var pathInput = document.getElementById("pathInput");
var statusEl = document.getElementById("status");
pathInput.addEventListener("keydown", function(e) { if (e.key === "Enter") scanDirectory(); });

function esc(s) { var d = document.createElement("div"); d.textContent = s; return d.innerHTML; }

function navigateTo(path) {
  pathInput.value = path;
  scanDirectory();
}

function goUp() {
  var p = currentPath.replace(/[/\\\\][^/\\\\]*$/, "") || "/";
  pathInput.value = p;
  scanDirectory();
}

function goHome() {
  fetch("/api/home")
    .then(function(r) { return r.json(); })
    .then(function(data) {
      pathInput.value = data.path || "/";
      scanDirectory();
    })
    .catch(function() {
      pathInput.value = "/";
      scanDirectory();
    });
}

function refresh() {
  scanDirectory();
}

function toggleSettings() {
  var panel = document.getElementById("settingsPanel");
  panel.classList.toggle("open");
}

function buildApiUrl(path) {
  var url = "/api/tree?path=" + encodeURIComponent(path);
  if (document.getElementById("showHidden").checked) {
    url += "&show_hidden=true";
  }
  return url;
}

document.addEventListener("click", function(e) {
  var panel = document.getElementById("settingsPanel");
  var btn = e.target.closest(".settings-btn");
  if (!btn && panel.classList.contains("open")) {
    panel.classList.remove("open");
  }
});

function scanDirectory() {
  var path = pathInput.value.trim();
  if (!path) return;
  currentPath = path;
  container.innerHTML = "<div class=\"loading\">Loading...</div>";
  statusEl.textContent = "Loading...";
  fetch(buildApiUrl(path))
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (data.error) { container.innerHTML = "<div class=\"error\">" + esc(data.error) + "</div>"; statusEl.textContent = "Error"; return; }
      renderTree(data);
      statusEl.textContent = path;
    })
    .catch(function(e) { container.innerHTML = "<div class=\"error\">Request failed: " + esc(e.message) + "</div>"; statusEl.textContent = "Error"; });
}

function renderTree(data) {
  container.innerHTML = "";
  var rootLine = createNodeEl(data, "", true, true);
  container.appendChild(rootLine);
  if (data.children) {
    for (var i = 0; i < data.children.length; i++) {
      var isLast = (i == data.children.length - 1);
      var prefix = isLast ? "    " : "\u2502   ";
      var childLine = createNodeEl(data.children[i], prefix, isLast, false);
      container.appendChild(childLine);
    }
  }
}

function createNodeEl(node, prefix, isLast, isRoot) {
  var line = document.createElement("div");
  line.className = "tree-line";
  line.dataset.path = node.path || node.name || "";
  line.dataset.type = node.type || "file";
  line.dataset.hasChildren = node.hasChildren ? "true" : "false";
  line.style.whiteSpace = "pre";

  if (isRoot) {
    line.textContent = (isLast ? "\u2514\u2500 " : "\u251c\u2500 ") + node.name + (node.name[node.name.length-1] !== "/" ? "/" : "");
    if (node.type === "directory" && node.hasChildren === true) {
      line.style.color = "#7aa2f7";
      line.style.fontWeight = "bold";
    }
    // Root click -> navigate
    if (node.type === "directory") {
      line.style.cursor = "pointer";
      line.onclick = function() { navigateTo(node.path || node.name); };
    }
  } else {
    var connector = isLast ? "\u2514\u2500 " : "\u251c\u2500 ";
    if (node.type === "directory") {
      var arrow = document.createElement("span");
      arrow.className = "tree-arrow";
      if (node.hasChildren === true) {
        arrow.textContent = "\u25b6";
        arrow.style.cursor = "pointer";
      } else {
        arrow.textContent = "  ";
      }
      arrow.style.marginRight = "2px";
      arrow._expanded = false;
      arrow._nodePath = node.path || "";
      arrow.onclick = node.hasChildren ? function() { toggleDir(this); } : null;
      line.appendChild(document.createTextNode(prefix + connector));
      line.appendChild(arrow);
      var nameSpan = document.createElement("span");
      nameSpan.className = "dir-link";
      nameSpan.textContent = node.name + (node.name[node.name.length-1] !== "/" ? "/" : "");
      nameSpan.onclick = function() { navigateTo(node.path || node.name); };
      line.appendChild(nameSpan);
    } else {
      line.textContent = prefix + connector + node.name;
    }
  }
  return line;
}

function toggleDir(arrowEl) {
  var lineEl = arrowEl.parentNode;
  if (arrowEl._expanded) {
    // Collapse: remove the children container (next sibling should be the container div)
    var next = lineEl.nextSibling;
    if (next && next.className === "children-container") {
      next.remove();
    }
    arrowEl.textContent = "\u25b6";
    arrowEl._expanded = false;
  } else {
    arrowEl.textContent = "\u25bc";
    arrowEl._expanded = true;
    var dirPath = arrowEl._nodePath;
    if (!dirPath) return;
    fetch(buildApiUrl(dirPath))
      .then(function(res) { return res.json(); })
      .then(function(data) {
        if (!data.children || data.children.length === 0) {
          arrowEl.textContent = "  ";
          arrowEl._expanded = false;
          return;
        }
        var container2 = document.createElement("div");
        container2.className = "children-container";
        // Calculate prefix for children
        var basePrefix = getPrefixOfLine(lineEl);
        for (var i = 0; i < data.children.length; i++) {
          var isLast = (i == data.children.length - 1);
          var childPrefix = basePrefix + (isLast ? "    " : "\u2502   ");
          var childLine = createNodeEl(data.children[i], childPrefix, isLast, false);
          container2.appendChild(childLine);
        }
        lineEl.parentNode.insertBefore(container2, lineEl.nextSibling);
      })
      .catch(function(e) { console.error("Expand failed:", e); arrowEl.textContent = "\u25b6"; arrowEl._expanded = false; });
  }
}

function getPrefixOfLine(lineEl) {
  var text = lineEl.textContent || "";
  var prefix = "";
  for (var i = 0; i < text.length; i++) {
    var c = text[i];
    if (c === " " || c === "\u2502" || c === "\u251c" || c === "\u2514" || c === "\u2500") {
      prefix += c;
    } else {
      break;
    }
  }
  return prefix;
}

</script>
</body>
</html>)html";
    }
};

// Out-of-line definition for ServiceManager::start (requires complete WebServer type)
bool ServiceManager::start(int port) {
    // Ensure config directory exists
    fs::create_directories(configDir());

    // If already running, stop first
    if (isRunning()) {
        std::cout << "[Service] Stopping existing instance...\n";
        stop();
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

#ifdef _WIN32
    // Windows: create detached process
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string exePath = getExecutablePath();
    std::string cmdLine = "\"" + exePath + "\" --web";
    // CreateProcessA may modify the command line string, use mutable buffer
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');
    if (CreateProcessA(exePath.c_str(), mutableCmd.data(),
        NULL, NULL, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
        NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        writePid(pi.dwProcessId);
        std::cout << "[Service] Started (PID: " << pi.dwProcessId << ")\n";
        return true;
    }
    std::cerr << "[Service] Failed to create process\n";
    return false;
#else
    // Unix: fork + setsid
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[Service] Fork failed: " << strerror(errno) << "\n";
        return false;
    }
    if (pid > 0) {
        // Parent: write PID and exit
        writePid(pid);
        std::cout << "[Service] Started in background (PID: " << pid << ")\n";
        std::cout << "[Service] Use 'tree --service off' to stop\n";
        exit(0);
    }
    // Child: become session leader, start WebServer
    setsid();
    WebServer server(port);
    server.start();
    return true;
#endif
}

class App {
public:
    App() {
        // Initialize terminal encoding at earliest opportunity
        TerminalEncoding::initialize();

    #ifdef _WIN32
        platform_ = std::make_unique<WindowsPlatformHelper>();
    #else
        platform_ = std::make_unique<UnixPlatformHelper>();
    #endif
    }

    void runTUI(bool settings_mode = false) {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hConsole, &mode))
            SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
        if (settings_mode) {
            showSettings();
            return;
        }
        
        fs::path current = fs::current_path();
        
        while (true) {
            std::cout << "\033[2J\033[H";
            std::cout << "=======================================\n";
            std::cout << "         TreeVisual TUI v1.1.2\n";
            std::cout << "=======================================\n";
            std::cout << "Current: " << current.string() << "\n\n";
            
            std::error_code ec;
            auto entries = listDirectorySimple(current);
            if (entries.empty()) {
                std::cout << "[Empty]\n";
            } else {
                int idx = 1;
                for (const auto& entry : entries) {
                    auto st = entry.status();
                    std::cout << "  " << idx << ". " << entry.path().filename().string();
                    if (fs::is_directory(st)) std::cout << "/";
                    std::cout << "\n";
                    idx++;
                }
            }
            
            std::cout << "\n[↑]Up [Enter]View [n]New [s]Settings [q]Quit\n";
            std::cout << "Choice: ";
            
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "q" || input == "Q") break;
            else if (input == "..") {
                current = current.parent_path();
            } else if (input == "n") {
                std::cout << "Enter path: ";
                std::getline(std::cin, input);
                if (!input.empty()) {
                    fs::path newpath(input);
                    if (fs::exists(newpath)) current = newpath;
                }
            } else if (input == "s" || input == "S") {
                showSettings();
            } else if (input.empty() || input == ".") {
                auto tree = std::make_unique<ParallelDirectoryTree>();
                auto root = tree->build(current, false);
                auto lines = TreeFormatter::formatLines(root, maxLines_);
                std::cout << "\n";
                for (auto& line : lines) std::cout << line << "\n";
                std::cout << "\n[Press Enter to continue]\n";
                std::getline(std::cin, input);
            }
        }
    }
    
    std::vector<fs::directory_entry> listDirectorySimple(const fs::path& dir) {
        std::vector<fs::directory_entry> entries;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                std::string an = a.path().filename().string();
                std::string bn = b.path().filename().string();
                std::transform(an.begin(), an.end(), an.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });
                std::transform(bn.begin(), bn.end(), bn.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });
                return an < bn;
            });
        return entries;
    }
    
    void showSettings() {
        while (true) {
            std::cout << "\033[2J\033[H";
            std::cout << "=======================================\n";
            std::cout << "         Settings\n";
            std::cout << "=======================================\n";
            std::cout << "1. Max Lines: " << maxLines_ << "\n";
            std::cout << "2. Back\n";
            std::cout << "\nSelect (1-2): ";

            std::string input;
            std::getline(std::cin, input);

            if (input == "1") {
                std::cout << "Max Lines: ";
                std::getline(std::cin, input);
                try { maxLines_ = std::stoi(input); } catch (...) {}
            } else if (input == "2" || input == "q") {
                break;
            }
        }
    }

    int run(int argc, char* argv[]) {
        bool elevated = false;
        bool show_hidden = false;
        bool tui_mode = false;
        bool settings_mode = false;
        int web_mode = 0;       // 0=none, 1=start(foreground), 2=start, 3=stop
        int service_mode = 0;   // 0=none, 1=on, 2=off, 3=status
        std::vector<std::string> userArgs;

        enum { WEB_NONE, WEB_START_FG, WEB_START_BG, WEB_STOP };
        enum { SVC_NONE, SVC_ON, SVC_OFF, SVC_STATUS };

        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--elevated") == 0)
                elevated = true;
            else if (strcmp(argv[i], "--hidden") == 0)
                show_hidden = true;
            else if (strcmp(argv[i], "-v") == 0)
                tui_mode = true;
            else if (strcmp(argv[i], "--setting") == 0)
                settings_mode = true;
            else if ((strcmp(argv[i], "--web") == 0 || strcmp(argv[i], "-w") == 0)) {
                // Check for sub-command: start/stop
                if (i + 1 < argc) {
                    if (strcmp(argv[i+1], "start") == 0) { web_mode = WEB_START_BG; ++i; }
                    else if (strcmp(argv[i+1], "stop") == 0) { web_mode = WEB_STOP; ++i; }
                    else web_mode = WEB_START_FG;  // default: foreground start
                } else {
                    web_mode = WEB_START_FG;
                }
            }
            else if (strcmp(argv[i], "--service") == 0) {
                // Check for sub-command: on/off/status
                if (i + 1 < argc) {
                    if (strcmp(argv[i+1], "on") == 0) { service_mode = SVC_ON; ++i; }
                    else if (strcmp(argv[i+1], "off") == 0) { service_mode = SVC_OFF; ++i; }
                    else if (strcmp(argv[i+1], "status") == 0) { service_mode = SVC_STATUS; ++i; }
                    else { std::cerr << "Unknown service sub-command: " << argv[i+1] << "\n"; return 1; }
                } else {
                    service_mode = SVC_ON;  // default --service → on
                }
            }
            else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                std::cout << "TreeVisual v1.1.2 - Directory Tree Visualizer\n\n"
                          << "Usage: tree [options] [path]\n\n"
                          << "Options:\n"
                          << "  --hidden    Show hidden files\n"
                          << "  -v          Interactive TUI mode\n"
                          << "  -w, --web [start|stop]   WebUI server control\n"
                          << "                              (default: start in foreground)\n"
                          << "  --service [on|off|status] Daemon service mode\n"
                          << "                              (default: on)\n"
                          << "  --setting   Open settings\n"
                          << "  --elevated  Re-run with admin privileges (internal)\n"
                          << "  -h, --help  Show this help message\n\n"
                          << "Examples:\n"
                          << "  tree --web              Start WebUI (foreground)\n"
                          << "  tree --web start        Start or restart WebUI\n"
                          << "  tree --web stop         Stop WebUI server\n"
                          << "  tree --service on       Start as background daemon\n"
                          << "  tree --service off      Stop the daemon service\n"
                          << "  tree --service status   Check if daemon is running\n\n"
                          << "If no path is specified, the current directory is used.\n";
                return 0;
            }
            else
                userArgs.push_back(argv[i]);
        }

        // Handle --service commands
        if (service_mode != SVC_NONE) {
            switch (service_mode) {
                case SVC_ON:
                    ServiceManager::start(7200);
                    break;
                case SVC_OFF:
                    ServiceManager::stop();
                    break;
                case SVC_STATUS:
                    if (ServiceManager::isRunning()) {
                        std::cout << "[Service] Running (PID file: "
                                  << ServiceManager::pidFilePath() << ")\n";
                    } else {
                        std::cout << "[Service] Not running\n";
                    }
                    break;
                default: break;
            }
            return 0;
        }

        // Handle --web commands
        if (web_mode != WEB_NONE) {
            switch (web_mode) {
                case WEB_STOP:
                    if (!ServiceManager::isRunning()) {
                        std::cout << "[WebUI] No running instance found.\n";
                        return 1;
                    }
                    ServiceManager::stop();
                    std::cout << "[WebUI] Server stopped.\n";
                    return 0;

                case WEB_START_BG:
                case WEB_START_FG:
                    // If already running and user wants start, restart
                    if (ServiceManager::isRunning()) {
                        std::cout << "[WebUI] Restarting existing instance...\n";
                        ServiceManager::stop();
#ifdef _WIN32
                        Sleep(1000);
#else
                        sleep(1);
#endif
                    }
                    {
                        WebServer server(7200);
                        server.start();
                    }
                    return 0;

                default: break;
            }
        }

        if (tui_mode || settings_mode) {
            runTUI(settings_mode);
            return 0;
        }

        fs::path target;
        if (!userArgs.empty())
            target = fs::absolute(fs::path(userArgs[0]));
        else
            target = fs::current_path();

        auto tree = std::make_unique<ParallelDirectoryTree>();
        auto root = tree->build(target, show_hidden);

        std::string treeText = TreeFormatter::format(root, show_hidden);
        size_t lineCount = std::count(treeText.begin(), treeText.end(), '\n');

        if (PermissionErrorTracker::instance().hasError() && !platform_->isAdmin() && !elevated) {
            std::cout << "\nWarning: Cannot access directory \""
                      << PermissionErrorTracker::instance().firstPath().string()
                      << "\", higher privileges required for a complete tree.\n";
            std::string choice;
            while (true) {
                std::cout << "Attempt to re-run with administrator/root privileges? (y/n): ";
                std::getline(std::cin, choice);
                if (choice == "y" || choice == "yes" || choice == "是") {
                    std::vector<std::string> newArgs = userArgs;
                    newArgs.push_back("--elevated");
                    if (show_hidden) newArgs.push_back("--hidden");
                    platform_->runAsAdmin(newArgs);
                    return 0;
                } else if (choice == "n" || choice == "no" || choice == "否" || choice.empty()) {
                    std::cout << "Continuing with normal privileges; inaccessible entries will be shown as [Permission denied].\n";
                    break;
                } else {
                    std::cout << "Please enter y or n.\n";
                }
            }
        }

        if (lineCount <= DISPLAY_LINE_THRESHOLD) {
            std::cout << treeText;
            ClipboardOutputHandler clipboard;
            if (clipboard.handle(treeText, lineCount))
                std::cout << "\n(" << lineCount << " lines, automatically copied to clipboard)\n";
        } else if (lineCount <= CLIPBOARD_LINE_THRESHOLD) {
            ClipboardOutputHandler clipboard;
            if (clipboard.handle(treeText, lineCount))
                std::cout << "Tree contains " << lineCount << " lines, automatically copied to clipboard (not displayed)\n";
            else
                std::cout << "Clipboard copy failed, displaying tree:\n" << treeText;
        } else {
            FileOutputHandler file_handler;
            file_handler.handle(treeText, lineCount);
        }

        return 0;
    }

private:
    std::unique_ptr<IPlatformHelper> platform_;
    int maxLines_ = 100;
};

int main(int argc, char* argv[]) {
    App app;
    return app.run(argc, argv);
}