# Private, pinned upstream runtimes. No downloads or application dependencies.
enable_language(C)
set(_rt)
foreach(_component lilv serd sord sratom zix)
  file(GLOB _files CONFIGURE_DEPENDS "third_party/${_component}/src/*.c")
  list(FILTER _files EXCLUDE REGEX "/(serdi|sordi|sord_validate)\\.c$")
  list(APPEND _rt ${_files})
endforeach()
if(WIN32)
  file(GLOB _platform "third_party/zix/src/win32/*.c")
else()
  file(GLOB _platform "third_party/zix/src/posix/*.c")
endif()
list(APPEND _rt ${_platform})
file(GLOB _ysfx CONFIGURE_DEPENDS "third_party/ysfx/sources/*.cpp")
list(APPEND _rt ${_ysfx})
if(WIN32)
  file(GLOB _gfx CONFIGURE_DEPENDS "third_party/ysfx/thirdparty/WDL/source/WDL/lice/*.cpp"
    "third_party/ysfx/sources/lice_stb/*.cpp")
  list(APPEND _rt ${_gfx})
endif()
foreach(_eel nseel-caltab nseel-cfunc nseel-compiler nseel-eval nseel-lextab nseel-ram nseel-yylex)
  list(APPEND _rt "third_party/ysfx/thirdparty/WDL/source/WDL/eel2/${_eel}.c")
endforeach()
list(APPEND _rt third_party/ysfx/thirdparty/WDL/source/WDL/fft.c)
if(WIN32)
  list(APPEND _rt third_party/ysfx/thirdparty/WDL/source/WDL/win32_utf8.c)
endif()
add_library(CyCLAPBridgeRuntimes OBJECT ${_rt})
set_target_properties(CyCLAPBridgeRuntimes PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(_runtimeIncludes third_party/lv2/include third_party/lilv/include third_party/serd/include
  third_party/sord/include third_party/sratom/include third_party/zix/include
  third_party/ysfx/include third_party/ysfx/sources third_party/ysfx/thirdparty/WDL/source
  third_party/ysfx/thirdparty/dr_libs third_party/ysfx/thirdparty/stb)
target_include_directories(CyCLAPBridgeRuntimes SYSTEM PRIVATE ${_runtimeIncludes})
target_include_directories(CyCLAPBridge SYSTEM PRIVATE ${_runtimeIncludes} third_party/vst3sdk)
target_compile_features(CyCLAPBridgeRuntimes PRIVATE cxx_std_20)
set(_runtimeDefinitions LILV_STATIC SERD_STATIC SORD_STATIC SRATOM_STATIC ZIX_STATIC
  EEL_TARGET_PORTABLE YSFX_NO_FTS WDL_FFT_REALSIZE=8
  WDL_LINEPARSE_ATOF=ysfx_wdl_atof NSEEL_ATOF=ysfx_wdl_atof
  _FILE_OFFSET_BITS=64 _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS NOMINMAX)
target_compile_definitions(CyCLAPBridgeRuntimes PRIVATE ${_runtimeDefinitions})
if(NOT WIN32)
  target_compile_definitions(CyCLAPBridgeRuntimes PRIVATE YSFX_NO_GFX)
endif()
target_compile_definitions(CyCLAPBridge PRIVATE LILV_STATIC SERD_STATIC SORD_STATIC SRATOM_STATIC ZIX_STATIC)
target_sources(CyCLAPBridge PRIVATE $<TARGET_OBJECTS:CyCLAPBridgeRuntimes>
  third_party/vst3sdk/pluginterfaces/base/coreiids.cpp
  third_party/vst3sdk/pluginterfaces/base/funknown.cpp
  third_party/vst3sdk/public.sdk/source/vst/vstinitiids.cpp)
if(MSVC)
  target_compile_options(CyCLAPBridgeRuntimes PRIVATE /W0 /utf-8)
else()
  target_compile_options(CyCLAPBridgeRuntimes PRIVATE -w -fsigned-char)
  target_compile_definitions(CyCLAPBridgeRuntimes PRIVATE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=600)
endif()
