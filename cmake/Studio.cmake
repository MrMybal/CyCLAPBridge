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
  set(CYUI_MODULES FilePicker)
  add_subdirectory(third_party/CyUI EXCLUDE_FROM_ALL)
  add_executable(CyCLAPBridgeStudio WIN32 MACOSX_BUNDLE
    apps/Studio.cpp apps/StudioModel.cpp apps/StudioPlatform.cpp)
  cybridge_application(CyCLAPBridgeStudio)
  target_link_libraries(CyCLAPBridgeStudio PRIVATE CyUI::Core CyUI::FilePicker
    cybridge_studio_imgui glfw OpenGL::GL)
  if(WIN32)
    target_compile_definitions(CyCLAPBridgeStudio PRIVATE NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_sources(CyCLAPBridgeStudio PRIVATE apps/windows/Studio.manifest)
    target_link_libraries(CyCLAPBridgeStudio PRIVATE shell32 shlwapi)
    if(MSVC)
      target_link_options(CyCLAPBridgeStudio PRIVATE "/MANIFESTINPUT:${PROJECT_SOURCE_DIR}/apps/windows/Studio.manifest")
    endif()
  elseif(APPLE)
    target_sources(CyCLAPBridgeStudio PRIVATE apps/macos/StudioPlatform.mm)
    set_source_files_properties(apps/macos/StudioPlatform.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_target_properties(CyCLAPBridgeStudio PROPERTIES
      OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON
      MACOSX_BUNDLE_GUI_IDENTIFIER org.cybridge.studio
      MACOSX_BUNDLE_BUNDLE_NAME "CyCLAPBridge Studio"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/apps/macos/Info.plist.in")
    target_link_libraries(CyCLAPBridgeStudio PRIVATE "-framework AppKit")
  endif()
  set(_ui_notices "${_cybridge_data_dir}/licenses/studio")
  install(FILES third_party/CyUI/LICENSE DESTINATION "${_ui_notices}" RENAME CyUI.txt)
  install(FILES third_party/glfw/LICENSE.md DESTINATION "${_ui_notices}" RENAME GLFW.txt)
  install(FILES third_party/imgui/LICENSE.txt DESTINATION "${_ui_notices}" RENAME Dear-ImGui.txt)
  install(FILES third_party/CyUI/third_party/jetbrains-mono/OFL.txt DESTINATION "${_ui_notices}" RENAME JetBrains-Mono.txt)
  install(FILES third_party/imgui/imstb_textedit.h third_party/imgui/imstb_truetype.h
    third_party/imgui/imstb_rectpack.h DESTINATION "${_ui_notices}/embedded-notices")
endfunction()
