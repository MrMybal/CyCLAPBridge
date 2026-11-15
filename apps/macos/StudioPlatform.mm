// SPDX-License-Identifier: MIT
#include "../StudioPlatform.h"
#import <AppKit/AppKit.h>
namespace cybridge::studio {
std::string fileUrl(const std::filesystem::path& path) {
    @autoreleasepool {
        auto text=pathText(std::filesystem::absolute(path));
        NSString* native=[[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
        if(!native)return {};
        const char* encoded=[[[NSURL fileURLWithPath:native] absoluteString] UTF8String];
        return encoded?std::string(encoded):std::string{};
    }
}
bool openUrl(const std::string& text) {
    if(text.rfind("file:",0)!=0)return false;
    @autoreleasepool {
        NSString* encoded=[[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
        NSURL* url=encoded?[NSURL URLWithString:encoded]:nil;
        return url&&[[NSWorkspace sharedWorkspace] openURL:url];
    }
}
}
