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
#include <cstdint>
#include <ctime>
#include <memory>
#include <functional>

#ifdef _WIN32
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
#endif

namespace fs = std::filesystem;

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
        root_ = buildNode(root, show_hidden);
        return root_;
    }

    const fs::path& rootPath() const { return root_path_; }
    void setRootPath(const fs::path& path) { root_path_ = path; }

protected:
    std::shared_ptr<Node> root_;
    fs::path root_path_;

    virtual std::shared_ptr<Node> buildNode(const fs::path& path, bool show_hidden) {
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
            auto dir = buildNode(entry.path(), show_hidden);
            node->addChild(dir);
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

        DefaultTraversalPolicy policy;
        auto entries = policy.listDirectory(root, show_hidden);
        
        std::string root_name = root.filename().string();
        if (root_name.empty()) root_name = root.string();
        root_ = std::make_shared<Node>(root_name, Node::Type::Directory);
        root_path_ = root;

        if (entries.empty()) {
            return root_;
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
            root_->addChild(file);
        }

        if (subdirs.empty()) {
            return root_;
        }

        unsigned int num_threads = (max_threads_ > 0) ? max_threads_ : 4;
        if (num_threads > static_cast<unsigned int>(subdirs.size())) {
            num_threads = static_cast<unsigned int>(subdirs.size());
        }

        std::vector<std::shared_ptr<Node>> sub_results(subdirs.size());
        std::atomic<size_t> next_idx(0);
        std::vector<std::thread> workers;

        for (unsigned int w = 0; w < num_threads; ++w) {
            workers.emplace_back([&, show_hidden]() {
                DefaultTraversalPolicy local_policy;
                while (true) {
                    size_t idx = next_idx.fetch_add(1);
                    if (idx >= subdirs.size()) break;
                    sub_results[idx] = buildSubTree(subdirs[idx].path(), show_hidden, local_policy);
                }
            });
        }
        for (auto& worker : workers) worker.join();

        for (size_t i = 0; i < subdirs.size(); ++i) {
            auto& sub = subdirs[i];
            auto sub_node = sub_results[i];
            auto named_node = std::make_shared<Node>(
                sub.path().filename().string() + "/", Node::Type::Directory);
            for (const auto& child : sub_node->children()) {
                named_node->addChild(child);
            }
            root_->addChild(named_node);
        }

        return root_;
    }

private:
    std::shared_ptr<Node> buildSubTree(const fs::path& path, bool show_hidden, DefaultTraversalPolicy& policy) {
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
            node->addChild(sub);
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
        const char* cmd = nullptr;
#ifdef __APPLE__
        cmd = "pbcopy";
#else
        if (system("which xclip >/dev/null 2>&1") == 0)
            cmd = "xclip -selection clipboard";
        else if (system("which xsel >/dev/null 2>&1") == 0)
            cmd = "xsel -i -b";
        else
            return false;
#endif
        FILE* pipe = popen(cmd, "w");
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
        oss << std::put_time(&tm, "%y-%m-%d-%H-%M-%S") << "-tree.txt";
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
            " © 2026 WinTerminal)";
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
        wchar_t* buf = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &buf))) {
            fs::path p(buf);
            CoTaskMemFree(buf);
            return p;
        }
        wchar_t* wprofile = _wgetenv(L"USERPROFILE");
        if (wprofile && wprofile[0] != L'\0')
            return fs::path(wprofile);
        throw std::runtime_error("Cannot determine user home directory");
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
            cmdLine += L"\"" + wArg + L"\"";
        }
        HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath, cmdLine.c_str(), nullptr, SW_NORMAL);
        return reinterpret_cast<intptr_t>(result) > 32;
    }
};

#else

class UnixPlatformHelper : public IPlatformHelper {
public:
    fs::path getHomeDir() override {
        const char* home = getenv("HOME");
        if (home && home[0] != '\0')
            return fs::path(home);
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0] != '\0')
            return fs::path(pw->pw_dir);
        throw std::runtime_error("Cannot determine user home directory");
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
        std::string exePath = fs::canonical("/proc/self/exe").string();
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
        std::string name = node->name();
        if (node->isDirectory() && name.find('/') == std::string::npos)
            name += "/";

        if (!prefix.empty()) {
            std::string connector = is_last ? "└─ " : "├─ ";
            lines.push_back(prefix + connector + name);
        } else {
            lines.push_back(name);
        }

        const auto& children = node->children();
        if (children.empty()) return;

        size_t count = children.size();
        for (size_t i = 0; i < count; ++i) {
            bool child_is_last = (i == count - 1);
            std::string child_prefix = prefix + (is_last ? "    " : "│   ");
            formatNode(children[i], child_prefix, child_is_last, lines);
        }
    }
};

class App {
public:
    App() {
    #ifdef _WIN32
        platform_ = std::make_unique<WindowsPlatformHelper>();
    #else
        platform_ = std::make_unique<UnixPlatformHelper>();
    #endif
    }

    void runTUI(bool settings_mode = false) {
        if (settings_mode) {
            showSettings();
            return;
        }
        
        fs::path current = fs::current_path();
        
        while (true) {
            std::cout << "\033[2J\033[H";
            std::cout << "=======================================\n";
            std::cout << "         TreeVisual TUI v1.0.1\n";
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
                auto lines = TreeFormatter::formatLines(root, 30);
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
        return entries;
    }
    
    void showSettings() {
        std::string theme = "Default";
        std::string outputMode = "Auto";
        int maxLines = 100;
        
        while (true) {
            std::cout << "\033[2J\033[H";
            std::cout << "=======================================\n";
            std::cout << "         Settings\n";
            std::cout << "=======================================\n";
            std::cout << "1. Theme: " << theme << "\n";
            std::cout << "2. Output Mode: " << outputMode << "\n";
            std::cout << "3. Max Lines: " << maxLines << "\n";
            std::cout << "4. Back\n";
            std::cout << "\nSelect (1-4): ";
            
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "1") {
                std::cout << "Theme (Default/Dark): ";
                std::getline(std::cin, theme);
            } else if (input == "2") {
                std::cout << "Output Mode (Auto/Clipboard/File): ";
                std::getline(std::cin, outputMode);
            } else if (input == "3") {
                std::cout << "Max Lines: ";
                std::getline(std::cin, input);
                try { maxLines = std::stoi(input); } catch (...) {}
            } else if (input == "4" || input == "q") {
                break;
            }
        }
    }

    int run(int argc, char* argv[]) {
        bool elevated = false;
        bool show_hidden = false;
        bool tui_mode = false;
        bool settings_mode = false;
        std::vector<std::string> userArgs;

        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--elevated") == 0)
                elevated = true;
            else if (strcmp(argv[i], "--hidden") == 0)
                show_hidden = true;
            else if (strcmp(argv[i], "-v") == 0)
                tui_mode = true;
            else if (strcmp(argv[i], "--setting") == 0)
                settings_mode = true;
            else
                userArgs.push_back(argv[i]);
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
};

int main(int argc, char* argv[]) {
    App app;
    return app.run(argc, argv);
}