# Studio is an optional consumer. None of these targets enter the library export.
function(cybridge_add_studio)
  foreach(component CyUI glfw imgui)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/${component}/CMakeLists.txt" AND NOT component STREQUAL "imgui")
      message(FATAL_ERROR "Studio needs pinned third_party/${component}; initialize its Git submodule or use CYCLAPBRIDGE_BUILD_GUI=OFF")
    endif()
  endforeach()
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.h")
    message(FATAL_ERROR "Initialize the pinned third_party/imgui submodule for Studio")
  endif()
  find_package(OpenGL REQUIRED)
  set(BUILD_SHARED_LIBS OFF)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  endif()
  add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)
  set(_imgui "${PROJECT_SOURCE_DIR}/third_party/imgui")
  add_library(cybridge_studio_imgui STATIC
    "${_imgui}/imgui.cpp" "${_imgui}/imgui_draw.cpp"
    "${_imgui}/imgui_tables.cpp" "${_imgui}/imgui_widgets.cpp"
    "${_imgui}/misc/cpp/imgui_stdlib.cpp"
    "${_imgui}/backends/imgui_impl_glfw.cpp" "${_imgui}/backends/imgui_impl_opengl3.cpp")
  target_include_directories(cybridge_studio_imgui PUBLIC "${_imgui}" "${_imgui}/backends" "${_imgui}/misc/cpp")
  target_link_libraries(cybridge_studio_imgui PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})
  set(CYUI_IMGUI_TARGET cybridge_studio_imgui)
  set(CYUI_BUILD_DEMO OFF)
  set(CYUI_BUILD_GALLERY OFF)
  set(CYUI_BUILD_AUDIO OFF)
  set(CYUI_BUILD_TESTS OFF)
  set(CYUI_BUILD_WEB_CEF OFF)
  set(CYUI_MODULES FilePicker DocumentViews)
  add_subdirectory(third_party/CyUI EXCLUDE_FROM_ALL)
  cybridge_add_operations()
  set(_studio_sdk_source "${_studio_sdk_source}" PARENT_SCOPE)
  set(_studio_sdk_revision "${_studio_sdk_revision}" PARENT_SCOPE)
  set(_studio_ui_notices
    "third_party/CyUI/third_party/md4c/LICENSE.md|studio/MD4C.md"
    "third_party/CyUI/third_party/lexilla/License.txt|studio/Lexilla.txt"
    "third_party/CyUI/third_party/scintilla/License.txt|studio/Scintilla.txt"
    "third_party/CyUI/third_party/stb/LICENSE|studio/stb.txt"
  )
  set(_studio_chat_notices "${_cybridge_sdk_notices};${_studio_ui_notices}" PARENT_SCOPE)
  set(_studio_targets CyCLAPBridgeStudio)
  if(CYCLAPBRIDGE_BUILD_TESTS)
    list(APPEND _studio_targets cybridge-studio-live-host)
  endif()
  foreach(_studio_target IN LISTS _studio_targets)
    add_executable(${_studio_target} WIN32 MACOSX_BUNDLE
      apps/Studio.cpp apps/StudioChat.cpp apps/StudioPlatform.cpp)
    if(_studio_target STREQUAL "CyCLAPBridgeStudio")
      cybridge_application(${_studio_target})
    else()
      target_link_libraries(${_studio_target} PRIVATE CyCLAPBridge::CyCLAPBridge)
      target_compile_definitions(${_studio_target} PRIVATE CYBRIDGE_STUDIO_LIVE_TEST)
      target_compile_options(${_studio_target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8>)
    endif()
    target_include_directories(${_studio_target} PRIVATE "${_studio_sdk_source}/examples/chat_app")
    target_link_libraries(${_studio_target} PRIVATE CyUI::Core CyUI::FilePicker
      cybridge_studio_imgui glfw OpenGL::GL CyMCP::ChatUI cybridge_studio_tools)
    if(WIN32)
      target_compile_definitions(${_studio_target} PRIVATE NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
      target_sources(${_studio_target} PRIVATE apps/windows/Studio.manifest)
      target_link_libraries(${_studio_target} PRIVATE shell32 shlwapi)
      if(MSVC)
        target_link_options(${_studio_target} PRIVATE "/MANIFESTINPUT:${PROJECT_SOURCE_DIR}/apps/windows/Studio.manifest")
      endif()
    elseif(APPLE)
      target_sources(${_studio_target} PRIVATE apps/macos/StudioPlatform.mm)
      set_source_files_properties(apps/macos/StudioPlatform.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
      set_target_properties(${_studio_target} PROPERTIES
        OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON
        MACOSX_BUNDLE_GUI_IDENTIFIER org.cybridge.studio
        MACOSX_BUNDLE_BUNDLE_NAME "CyCLAPBridge Studio"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/apps/macos/Info.plist.in")
      target_link_libraries(${_studio_target} PRIVATE "-framework AppKit")
    endif()
  endforeach()
  foreach(_notice IN LISTS _studio_ui_notices)
    string(REPLACE "|" ";" _pair "${_notice}")
    list(GET _pair 0 _input)
    list(GET _pair 1 _output)
    get_filename_component(_notice_name "${_output}" NAME)
    install(FILES "${_input}" DESTINATION "${_cybridge_data_dir}/licenses/studio" RENAME "${_notice_name}")
  endforeach()
  set(_ui_notices "${_cybridge_data_dir}/licenses/studio")
  install(FILES third_party/CyUI/LICENSE DESTINATION "${_ui_notices}" RENAME CyUI.txt)
  install(FILES third_party/glfw/LICENSE.md DESTINATION "${_ui_notices}" RENAME GLFW.txt)
  install(FILES third_party/imgui/LICENSE.txt DESTINATION "${_ui_notices}" RENAME Dear-ImGui.txt)
  install(FILES third_party/CyUI/third_party/jetbrains-mono/OFL.txt DESTINATION "${_ui_notices}" RENAME JetBrains-Mono.txt)
  install(FILES third_party/imgui/imstb_textedit.h third_party/imgui/imstb_truetype.h
    third_party/imgui/imstb_rectpack.h DESTINATION "${_ui_notices}/embedded-notices")
endfunction()
