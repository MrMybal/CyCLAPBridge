// SPDX-License-Identifier: MIT
#include "StudioPlatform.h"
#include "AppPaths.h"
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#elif !defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <cerrno>
#include <thread>
extern char** environ;
#endif
namespace cybridge::studio {
std::string pathText(const std::filesystem::path& path) {
    auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};
}
std::filesystem::path pathFromText(const std::string& text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
#ifndef __APPLE__
std::string fileUrl(const std::filesystem::path& path) {
#ifdef _WIN32
    std::wstring out(98304,L'\0');DWORD count=static_cast<DWORD>(out.size());
    if(FAILED(UrlCreateFromPathW(std::filesystem::absolute(path).c_str(),out.data(),&count,0)))return {};
    out.resize(wcslen(out.c_str()));
    const auto bytes=WideCharToMultiByte(CP_UTF8,0,out.data(),static_cast<int>(out.size()),nullptr,0,nullptr,nullptr);
    std::string result(bytes,'\0');WideCharToMultiByte(CP_UTF8,0,out.data(),static_cast<int>(out.size()),result.data(),bytes,nullptr,nullptr);return result;
#else
    // RFC 8089: encode UTF-8 path bytes, preserving only unreserved bytes and '/'.
    const auto value=pathText(std::filesystem::absolute(path));
    constexpr char hex[]="0123456789ABCDEF";
    std::string result="file://";
    for(unsigned char c:value) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'||c=='~')result+=static_cast<char>(c);
        else{result+='%';result+=hex[c>>4];result+=hex[c&15];}
    }
    return result;
#endif
}
bool openUrl(const std::string& url) {
    if(url.rfind("file:",0)!=0)return false;
#ifdef _WIN32
    auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url.data(),static_cast<int>(url.size()),nullptr,0);
    if(!count)return false;std::wstring text(count,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url.data(),static_cast<int>(url.size()),text.data(),count);
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",text.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;
#else
    // Direct argv, never a shell. Reap xdg-open without blocking the UI loop.
    std::string executable="xdg-open",value=url;
    char* argv[]{executable.data(),value.data(),nullptr};pid_t child=0;
    if(posix_spawnp(&child,executable.c_str(),nullptr,nullptr,argv,environ)!=0)return false;
    std::thread([child]{int status;while(waitpid(child,&status,0)<0&&errno==EINTR){}}).detach();
    return true;
#endif
}
#endif
std::filesystem::path settingsFile() { return app::settingsFile(); }
std::vector<std::string> roots() {
#ifdef _WIN32
    std::vector<std::string> result;auto mask=GetLogicalDrives();
    for(int i=0;i<26;++i)if(mask&(1u<<i))result.push_back(std::string(1,static_cast<char>('A'+i))+":/");
    return result;
#else
    return {"/"};
#endif
}
}
