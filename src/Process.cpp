// SPDX-License-Identifier: MIT
#include "Process.h"
#include <array>
#include <exception>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace cybridge::detail {
std::string utf8(const std::filesystem::path& p) {
    auto s = p.generic_u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
std::filesystem::path fromUtf8(const std::string& s) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}
#ifdef _WIN32
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring quote(const std::wstring& input) {
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (auto c : input) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
}
int runProcess(const std::filesystem::path& executable, const std::vector<std::string>& args,
               const LogSink& log) {
    std::wstring command = quote(executable.wstring());
    for (const auto& arg : args) command += L" " + quote(fromUtf8(arg).wstring());
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle read, write;
    if (!CreatePipe(&read.value, &write.value, &security, 0) ||
        !SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Cannot create build output pipe");
    Handle nullInput{CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 &security, OPEN_EXISTING, 0, nullptr)};
    if (nullInput.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open build stdin");
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<std::byte> storage(bytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes))
        throw std::runtime_error("Cannot initialize process attributes");
    struct Cleanup {
        LPPROC_THREAD_ATTRIBUTE_LIST p;
        ~Cleanup() { DeleteProcThreadAttributeList(p); }
    } cleanup{attributes};
    HANDLE inherited[]{write.value, nullInput.value};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited, sizeof(inherited), nullptr, nullptr))
        throw std::runtime_error("Cannot restrict inherited build handles");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = nullInput.value;
    startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = write.value;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                        &startup.StartupInfo, &info))
        throw std::runtime_error("Cannot launch CMake (Windows error " + std::to_string(GetLastError()) + ")");
    Handle process{info.hProcess}, thread{info.hThread};
    CloseHandle(write.value);
    write.value = INVALID_HANDLE_VALUE;
    std::array<char, 4096> buffer{};
    DWORD count = 0;
    std::exception_ptr callbackError;
    while (ReadFile(read.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count) {
        if (log && !callbackError) {
            try { log(std::string(buffer.data(), count)); }
            catch (...) { callbackError = std::current_exception(); }
        }
    }
    WaitForSingleObject(process.value, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.value, &code);
    if (callbackError) std::rethrow_exception(callbackError);
    return static_cast<int>(code);
}
#else
int runProcess(const std::filesystem::path& executable, const std::vector<std::string>& args,
               const LogSink& log) {
    int pipeFds[2];
    if (pipe(pipeFds)) throw std::runtime_error("Cannot create build output pipe");
    struct Fds {
        int a, b;
        ~Fds() { if(a >= 0) close(a); if(b >= 0) close(b); }
    } fds{pipeFds[0], pipeFds[1]};
    std::vector<std::string> strings{utf8(executable)};
    strings.insert(strings.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& s : strings) argv.push_back(s.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    struct Actions {
        posix_spawn_file_actions_t* p;
        ~Actions() { posix_spawn_file_actions_destroy(p); }
    } cleanup{&actions};
    posix_spawn_file_actions_adddup2(&actions, fds.b, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fds.b, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds.a);
    posix_spawn_file_actions_addclose(&actions, fds.b);
    pid_t pid = 0;
    auto error = posix_spawnp(&pid, strings[0].c_str(), &actions, nullptr, argv.data(), environ);
    if (error) throw std::runtime_error("Cannot launch CMake (error " + std::to_string(error) + ")");
    close(fds.b); fds.b = -1;
    std::array<char,4096> buffer{};
    std::exception_ptr callbackError;
    for (;;) {
        auto count = read(fds.a, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        if(log && !callbackError) {
            try { log(std::string(buffer.data(), static_cast<std::size_t>(count))); }
            catch (...) { callbackError = std::current_exception(); }
        }
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if(errno != EINTR) throw std::runtime_error("Cannot wait for CMake");
    if (callbackError) std::rethrow_exception(callbackError);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
#endif
}
