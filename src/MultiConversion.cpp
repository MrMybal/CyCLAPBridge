// SPDX-License-Identifier: MIT
#include <cyclapbridge/Conversion.h>
#include "Process.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
namespace cybridge::detail {
namespace fs=std::filesystem;
namespace {
std::string literal(const std::string& s){
    if(s.find_first_of(";\r\n")!=std::string::npos||s.find('\0')!=std::string::npos)throw std::invalid_argument("Unsupported control character or semicolon");
    std::string equal="=";while(s.find("]"+equal+"]")!=std::string::npos)equal+="=";return "["+equal+"["+s+"]"+equal+"]";
}
std::string cpp(const std::string& s){
    std::string out="\"";for(unsigned char c:s){if(c<32)throw std::invalid_argument("Control character in metadata");if(c=='"'||c=='\\')out+='\\';out+=static_cast<char>(c);}return out+"\"";
}
void write(const fs::path& p,const std::string& s){std::ofstream file(p,std::ios::binary);file.exceptions(std::ios::failbit|std::ios::badbit);file<<s;}
std::string identity(const std::string& text){
    std::uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}
    std::ostringstream s;s<<std::hex<<hash;return s.str();
}
bool inside(const fs::path& child,const fs::path& parent){
    auto c=fs::weakly_canonical(child),p=fs::canonical(parent);auto a=c.begin(),b=p.begin();
    for(;b!=p.end();++a,++b)if(a==c.end()||*a!=*b)return false;return true;
}
}
ConversionProject createExtendedConversionProject(const ConversionRequest& r){
    if(r.source==r.target)throw std::invalid_argument("Source and destination formats are identical");
    if(r.target==Format::jsfx)throw std::invalid_argument("Export to JSFX cannot embed a native binary in an EEL2 script. Use JSFX as a source, or a native REAPER extension.");
    if(r.source==Format::auv3)throw std::invalid_argument("AUv3 input is an installed Apple app extension, not a freely loadable file; use an AUv2 component");
    auto input=fs::canonical(r.input),destination=fs::absolute(r.projectDirectory).lexically_normal();
    if(fs::exists(destination))throw std::invalid_argument("Destination already exists");
    if(fs::is_directory(input)&&inside(destination,input))throw std::invalid_argument("Output must be outside the source bundle");
    auto root=r.backend.projectRoot.empty()?r.backend.templates.parent_path():r.backend.projectRoot;
    root=fs::canonical(root);
    if(!fs::exists(root/"src/HostedPlugin.cpp"))throw std::runtime_error("Native conversion needs the packaged CyCLAPBridge C++ SDK");
    auto name=r.outputName.empty()?utf8(input.stem()):r.outputName;
    if(name.empty()||name=="."||name==".."||name.size()>120||name.find_first_of("/\\:*?\"<>|$")!=std::string::npos||name.back()=='.'||name.back()==' ')
        throw std::invalid_argument("Invalid output filename");
    literal(name);literal(utf8(destination));
    auto payloadName=utf8(input.filename());
    if(payloadName.find_first_of("\";$\\\r\n")!=std::string::npos)throw std::invalid_argument("Unsupported source filename");
    if(r.target==Format::auv2&&!fs::exists(r.backend.audioUnitSdk/"include/AudioUnitSDK/AUBase.h") &&
       !fs::exists(r.backend.audioUnitSdk/"src/AudioUnitSDK/AUBase.h"))
        throw std::invalid_argument("AUv2 requires an AudioUnitSDK path (--au-sdk)");
    auto uid=identity(formatName(r.source)+":"+payloadName+":"+r.pluginId);
    auto pluginId="org.cybridge."+uid,uri="urn:cybridge:"+uid;
    auto temp=destination.parent_path()/(".cybridge-"+uid+"-"+std::to_string(std::random_device{}()));
    fs::create_directories(destination.parent_path());if(!fs::create_directory(temp))throw std::runtime_error("Staging directory collision");
    try{
        fs::create_directory(temp/"payload");
        fs::path local=input.filename();
        if(!r.resources.empty()){
            auto resources=fs::canonical(r.resources);
            if(!inside(input,resources)||inside(destination,resources))throw std::invalid_argument("Resources must contain the source and exclude the output");
            fs::copy(resources,temp/"payload/Resources",fs::copy_options::recursive);local=fs::path("Resources")/fs::relative(input,resources);
        }else fs::copy(input,temp/"payload"/local,fs::copy_options::recursive);
        auto relative=utf8(fs::path("CyCLAPBridge-payload")/local);
        std::ostringstream config;
        config<<"#pragma once\n#define CYBRIDGE_SOURCE_FORMAT "<<cpp(formatName(r.source))
              <<"\n#define CYBRIDGE_SOURCE_ID "<<cpp(r.pluginId)<<"\n#define CYBRIDGE_SOURCE_FILE "<<cpp(relative)
              <<"\n#define CYBRIDGE_PLUGIN_ID "<<cpp(pluginId)<<"\n#define CYBRIDGE_PLUGIN_URI "<<cpp(uri)
              <<"\n#define CYBRIDGE_PLUGIN_NAME "<<cpp(name)<<"\n";
        write(temp/"NativeConfig.h",config.str());
        fs::copy_file(root/"templates/NativeEntry.cpp",temp/"NativeEntry.cpp");
        fs::copy_file(root/"templates/NativeEditor.inc",temp/"NativeEditor.inc");
        std::ostringstream cm;
        cm<<"cmake_minimum_required(VERSION 3.27)\nproject(CyBridgeConversion VERSION 0.2.0 LANGUAGES C CXX)\n"
              "set(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
              "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")\n"
              "set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING \"\")\n"
              "set(CYCLAPBRIDGE_BUILD_GUI OFF CACHE BOOL \"\" FORCE)\nset(CYCLAPBRIDGE_BUILD_CLI OFF CACHE BOOL \"\" FORCE)\n"
              "set(CYCLAPBRIDGE_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\nset(CYCLAPBRIDGE_INSTALL_BACKEND OFF CACHE BOOL \"\" FORCE)\n"
           <<"add_subdirectory("<<literal(utf8(root))<<" cybridge EXCLUDE_FROM_ALL)\n"
              "set(_artifacts \"${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>\")\n";
        auto originalClap=r.source==Format::clap&&r.pluginId.empty();
        auto wrapperTarget=r.target==Format::vst3||r.target==Format::auv2||r.target==Format::auv3;
        if(wrapperTarget){
            cm<<"set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL \"\" FORCE)\nset(CLAP_WRAPPER_COPY_AFTER_BUILD OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_BUILD_AAX OFF CACHE BOOL \"\" FORCE)\n"
              <<"set(CLAP_WRAPPER_OUTPUT_NAME "<<literal(name)<<")\nset(CLAP_SDK_ROOT "<<literal(utf8(r.backend.clapSdk))<<" CACHE PATH \"\" FORCE)\n"
              <<"set(VST3_SDK_ROOT "<<literal(utf8(r.backend.vst3Sdk))<<" CACHE PATH \"\" FORCE)\n";
            if(r.target==Format::auv2)cm<<"set(AUDIOUNIT_SDK_ROOT "<<literal(utf8(r.backend.audioUnitSdk))<<" CACHE PATH \"\" FORCE)\n";
            cm<<"add_subdirectory("<<literal(utf8(r.backend.wrapper))<<" wrapper EXCLUDE_FROM_ALL)\n";
        }
        std::string entry="NativeEntry.cpp";
        if(originalClap&&wrapperTarget){
            fs::copy_file(root/"templates/PayloadEntry.cpp",temp/"PayloadEntry.cpp");
            write(temp/"PayloadName.h","#pragma once\n#define CYBRIDGE_PAYLOAD_NAME "+cpp(relative)+"\n");
            entry="PayloadEntry.cpp";
        }
        if(r.target==Format::lv2){
            fs::copy_file(root/"templates/Lv2Entry.cpp",temp/"Lv2Entry.cpp");
            fs::copy_file(root/"templates/Lv2Metadata.cpp",temp/"Lv2Metadata.cpp");
            cm<<"add_executable(cybridge_metadata Lv2Metadata.cpp)\ntarget_link_libraries(cybridge_metadata PRIVATE CyCLAPBridge::CyCLAPBridge)\n"
                 "target_include_directories(cybridge_metadata PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}\")\n"
                 "set(_metadata \"${CMAKE_CURRENT_BINARY_DIR}/metadata\")\nfile(MAKE_DIRECTORY \"${_metadata}\")\n"
                 "add_custom_command(OUTPUT \"${_metadata}/Lv2Metadata.h\" \"${_metadata}/plugin.ttl\"\n"
              <<" COMMAND $<TARGET_FILE:cybridge_metadata> \"${CMAKE_CURRENT_SOURCE_DIR}/payload/"<<utf8(local)<<"\" \"${_metadata}\"\n"
                 " DEPENDS cybridge_metadata VERBATIM)\n"
                 "add_library(cybridge_wrapper MODULE Lv2Entry.cpp \"${_metadata}/Lv2Metadata.h\")\n"
                 "target_include_directories(cybridge_wrapper PRIVATE \"${_metadata}\" "<<literal(utf8(root/"third_party/lv2/include"))<<")\n"
                 "set_target_properties(cybridge_wrapper PROPERTIES PREFIX \"\" OUTPUT_NAME \"plugin\" LIBRARY_OUTPUT_DIRECTORY \"${_artifacts}/"<<name<<".lv2\" RUNTIME_OUTPUT_DIRECTORY \"${_artifacts}/"<<name<<".lv2\")\n"
                 "set(_payload_destination \"$<TARGET_FILE_DIR:cybridge_wrapper>\")\n"
                 "file(GENERATE OUTPUT \"${CMAKE_CURRENT_BINARY_DIR}/manifest-$<CONFIG>.ttl\" CONTENT \"@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\\n@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\\n<"<<uri<<"> a lv2:Plugin ; lv2:binary <$<TARGET_FILE_NAME:cybridge_wrapper>> ; rdfs:seeAlso <plugin.ttl> .\\n\")\n"
                 "add_custom_command(TARGET cybridge_wrapper POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${_metadata}/plugin.ttl\" \"${_payload_destination}/plugin.ttl\"\n"
                 " COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${CMAKE_CURRENT_BINARY_DIR}/manifest-$<CONFIG>.ttl\" \"${_payload_destination}/manifest.ttl\" VERBATIM)\n";
        }else{
            if(r.target==Format::auv3)cm<<"if(NOT APPLE OR NOT CMAKE_GENERATOR STREQUAL \"Xcode\")\n message(FATAL_ERROR \"AUv3 requires macOS and the Xcode generator\")\nendif()\nadd_executable(cybridge_wrapper "<<entry<<")\n";
            else cm<<"add_library(cybridge_wrapper MODULE "<<entry<<")\n";
            if(r.target==Format::clap)cm<<"set_target_properties(cybridge_wrapper PROPERTIES PREFIX \"\" SUFFIX \".clap\" OUTPUT_NAME "<<literal(name)<<")\n";
            else if(r.target==Format::vst3)cm<<"target_add_vst3_wrapper(TARGET cybridge_wrapper OUTPUT_NAME \"${CLAP_WRAPPER_OUTPUT_NAME}\" WINDOWS_FOLDER_VST3 FALSE)\n";
            else if(r.target==Format::auv2||r.target==Format::auv3){
                auto subtype=r.subtype.empty()?uid.substr(0,4):r.subtype;
                if(r.manufacturer.size()!=4||subtype.size()!=4)throw std::invalid_argument("AU manufacturer and subtype must contain four characters");
                cm<<"if(NOT APPLE)\nmessage(FATAL_ERROR \"Audio Units require macOS\")\nendif()\n"
                  <<(r.target==Format::auv2?"target_add_auv2_wrapper(":"target_add_auv3_wrapper(")
                  <<"TARGET cybridge_wrapper OUTPUT_NAME \"${CLAP_WRAPPER_OUTPUT_NAME}\" BUNDLE_IDENTIFIER "<<literal(r.bundleId+"."+uid)
                  <<" BUNDLE_VERSION \"0.2.0\" MANUFACTURER_NAME \"CyCLAPBridge\" MANUFACTURER_CODE "<<literal(r.manufacturer)
                  <<" SUBTYPE_CODE "<<literal(subtype)<<" INSTRUMENT_TYPE \"aufx\" PREFER_CMAKE_AUV2_CONFIGURATION TRUE)\n";
            }
            if(r.target==Format::clap)cm<<"if(APPLE)\nset_target_properties(cybridge_wrapper PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap MACOSX_BUNDLE_GUI_IDENTIFIER "<<literal(r.bundleId+"."+uid)<<")\nendif()\n";
            cm<<"set_target_properties(cybridge_wrapper PROPERTIES LIBRARY_OUTPUT_DIRECTORY \"${_artifacts}\" RUNTIME_OUTPUT_DIRECTORY \"${_artifacts}\")\n"
                 "if(APPLE)\nset(_payload_destination \"$<TARGET_BUNDLE_DIR:cybridge_wrapper>/Contents/Resources\")\n"
                 "else()\nset(_payload_destination \"$<TARGET_FILE_DIR:cybridge_wrapper>\")\nendif()\n";
            if(wrapperTarget)cm<<"target_compile_definitions(clap-wrapper-shared-detail PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)\ntarget_compile_definitions(cybridge_wrapper PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)\n";
            if(r.target==Format::auv2||r.target==Format::auv3){
                auto helper=r.target==Format::auv2?"cybridge_wrapper-build-helper":"cybridge_wrapper-auv3-build-helper";
                cm<<"target_sources("<<helper<<" PRIVATE "<<entry<<")\ntarget_link_libraries("<<helper<<" PRIVATE CyCLAPBridge::CyCLAPBridge)\n"
                  <<"target_include_directories("<<helper<<" PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}\")\n";
            }
        }
        cm<<"target_link_libraries(cybridge_wrapper PRIVATE CyCLAPBridge::CyCLAPBridge)\n"
             "target_include_directories(cybridge_wrapper PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}\" "<<literal(utf8(r.backend.clapSdk/"include"))<<")\n"
             "if(MSVC)\ntarget_compile_options(cybridge_wrapper PRIVATE /utf-8)\nendif()\n"
             "add_custom_command(TARGET cybridge_wrapper POST_BUILD\n"
             "COMMAND ${CMAKE_COMMAND} -E copy_directory \"${CMAKE_CURRENT_SOURCE_DIR}/payload\" \"${_payload_destination}/CyCLAPBridge-payload\"\n"
             "COMMAND ${CMAKE_COMMAND} -E copy_directory \"${CMAKE_CURRENT_SOURCE_DIR}/licenses\" \"${_payload_destination}/CyCLAPBridge-licenses\" VERBATIM)\n";
        if(r.target==Format::auv3){
            auto subtype=r.subtype.empty()?uid.substr(0,4):r.subtype;
            cm<<"add_executable(cybridge_container)\ntarget_add_auv3_standalone_wrapper(TARGET cybridge_container OUTPUT_NAME "<<literal(name+" Host")
              <<" BUNDLE_IDENTIFIER "<<literal(r.bundleId+"."+uid+".host")<<" BUNDLE_VERSION \"0.2.0\" AUV3_TARGET cybridge_wrapper AU_TYPE \"aufx\" AU_SUBTYPE "<<literal(subtype)<<" AU_MANUFACTURER "<<literal(r.manufacturer)<<")\n"
                "set_target_properties(cybridge_container PROPERTIES RUNTIME_OUTPUT_DIRECTORY \"${_artifacts}\")\n"
                "add_custom_target(cybridge_all DEPENDS cybridge_wrapper cybridge_container)\n";
            write(temp/"cybridge-target.txt","cybridge_all\n");
        }
        fs::create_directory(temp/"licenses");
        fs::copy_file(root/"LICENSE",temp/"licenses/CyCLAPBridge.txt");
        fs::copy_file(root/"docs/Licensing.md",temp/"licenses/Licensing.md");
        fs::copy_file(root/"THIRD_PARTY.md",temp/"licenses/THIRD_PARTY.md");
        fs::copy(root/"third_party/licenses",temp/"licenses/runtimes",fs::copy_options::recursive);
        fs::copy_file(root/"third_party/clap/LICENSE",temp/"licenses/clap.txt");
        fs::copy_file(root/"third_party/clap-wrapper/LICENSE",temp/"licenses/clap-wrapper.txt");
        fs::copy_file(root/"third_party/vst3sdk/LICENSE.txt",temp/"licenses/vst3sdk.txt");
        fs::copy_file(root/"third_party/vst3sdk/base/LICENSE.txt",temp/"licenses/vst3-base.txt");
        fs::copy_file(root/"third_party/vst3sdk/pluginterfaces/LICENSE.txt",temp/"licenses/vst3-interfaces.txt");
        fs::copy_file(root/"third_party/vst3sdk/public.sdk/LICENSE.txt",temp/"licenses/vst3-public-sdk.txt");
        fs::copy_file(root/"third_party/clap-wrapper/libs/fmt/fmt/format.h",temp/"licenses/fmt-format.h");
        fs::copy_file(root/"third_party/clap-wrapper/libs/psl/presonus-plugin-extensions/ipslgainreduction.h",temp/"licenses/presonus-extensions.h");
        if(r.target==Format::auv2&&fs::exists(r.backend.audioUnitSdk/"LICENSE.txt"))fs::copy_file(r.backend.audioUnitSdk/"LICENSE.txt",temp/"licenses/AudioUnitSDK.txt");
        write(temp/"CMakeLists.txt",cm.str());
        write(temp/"cybridge-project.txt","CyCLAPBridge 0.2.0\n"+formatName(r.source)+" -> "+formatName(r.target)+"\nID: "+pluginId+"\nInput selection: "+r.pluginId+"\nThe wrapper loads plugin code in the host process selected by the DAW.\nThe DAW owns process isolation, crash recovery and security sandboxing.\nBuild steps execute the selected plugin for LV2 metadata.\n");
        if(fs::exists(destination))throw std::runtime_error("Destination appeared during generation");
        fs::rename(temp,destination);
    }catch(...){std::error_code ignored;fs::remove_all(temp,ignored);throw;}
    return {destination,name};
}
}
