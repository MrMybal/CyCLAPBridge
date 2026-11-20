add_library(cybridge-gain-fixture MODULE GainFixture.cpp)
target_include_directories(cybridge-gain-fixture PRIVATE ../third_party/clap/include)
target_compile_features(cybridge-gain-fixture PRIVATE cxx_std_20)
set_target_properties(cybridge-gain-fixture PROPERTIES PREFIX "" SUFFIX ".clap" OUTPUT_NAME "CyBridge Gain")
add_executable(cybridge-tests Tests.cpp)
target_link_libraries(cybridge-tests PRIVATE CyCLAPBridge::CyCLAPBridge)
target_compile_definitions(cybridge-tests PRIVATE CYBRIDGE_TEST_ROOT="${CMAKE_SOURCE_DIR}")
if(MSVC)
  target_compile_options(cybridge-tests PRIVATE /utf-8)
endif()
add_test(NAME cybridge-native COMMAND cybridge-tests "$<TARGET_FILE:cybridge-gain-fixture>" "${CMAKE_BINARY_DIR}/test-work")
add_executable(cybridge-multi-hosts MultiHosts.cpp)
target_link_libraries(cybridge-multi-hosts PRIVATE CyCLAPBridge::CyCLAPBridge)
add_test(NAME cybridge-jsfx-host COMMAND cybridge-multi-hosts jsfx "${CMAKE_CURRENT_SOURCE_DIR}/Gain.jsfx")
if(TARGET cyclapbridge-cli)
  add_test(NAME cybridge-cli-tools COMMAND "${CMAKE_COMMAND}"
    "-DCLI=$<TARGET_FILE:cyclapbridge-cli>" "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/Gain.jsfx"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/StudioCli.cmake")
  set(_live_cli "$<TARGET_FILE:cyclapbridge-cli>")
endif()
if(TARGET CyCLAPBridgeStudio)
  add_test(NAME cybridge-studio-smoke COMMAND CyCLAPBridgeStudio --smoke-test)
  add_executable(cybridge-studio-model StudioModelTests.cpp ../apps/StudioModel.cpp ../apps/StudioPlatform.cpp)
  target_include_directories(cybridge-studio-model PRIVATE ../apps)
  target_link_libraries(cybridge-studio-model PRIVATE CyCLAPBridge::CyCLAPBridge)
  if(WIN32)
    target_compile_definitions(cybridge-studio-model PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cybridge-studio-model PRIVATE shell32 shlwapi)
  elseif(APPLE)
    target_sources(cybridge-studio-model PRIVATE ../apps/macos/StudioPlatform.mm)
    target_link_libraries(cybridge-studio-model PRIVATE "-framework AppKit")
  endif()
  if(MSVC)
    target_compile_options(cybridge-studio-model PRIVATE /utf-8)
  endif()
  add_test(NAME cybridge-studio-controller COMMAND cybridge-studio-model "${PROJECT_SOURCE_DIR}"
    "$<TARGET_FILE:cybridge-gain-fixture>" "${CMAKE_BINARY_DIR}/test-work")
  add_test(NAME cybridge-studio-render COMMAND CyCLAPBridgeStudio --render-test)
  add_test(NAME cybridge-studio-chat-render COMMAND CyCLAPBridgeStudio --render-test --chat)
  add_executable(cybridge-studio-mcp StudioMcpTests.cpp)
  target_link_libraries(cybridge-studio-mcp PRIVATE cybridge_studio_tools CyMCP::ChatUI)
  target_include_directories(cybridge-studio-mcp SYSTEM PRIVATE
    "${_studio_sdk_source}/third_party/httplib" "${_studio_sdk_source}/examples/chat_app")
  if(MSVC)
    target_compile_options(cybridge-studio-mcp PRIVATE /utf-8)
  endif()
  add_test(NAME cybridge-studio-mcp COMMAND cybridge-studio-mcp
    "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Gain.jsfx"
    "${CMAKE_BINARY_DIR}/test-work/mcp" "${CMAKE_COMMAND}")
  set_tests_properties(cybridge-studio-mcp PROPERTIES TIMEOUT 480)
  add_test(NAME cybridge-studio-history-upgrade COMMAND cybridge-studio-mcp --restore-legacy
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/legacy-056.json" "${CMAKE_BINARY_DIR}/test-work/history")
  set_tests_properties(cybridge-studio-history-upgrade PROPERTIES TIMEOUT 30)
  foreach(profile IN ITEMS free local_controlled token_only protected)
    add_test(NAME cybridge-studio-live-mcp-${profile}
      COMMAND cybridge-studio-live-host --mcp-live-test "$<TARGET_FILE:cybridge-studio-mcp>"
      "${CMAKE_CURRENT_SOURCE_DIR}/Gain.jsfx" "${CMAKE_BINARY_DIR}/live"
      "${CMAKE_COMMAND}" ${profile} ${_live_cli})
    set_tests_properties(cybridge-studio-live-mcp-${profile} PROPERTIES TIMEOUT 480)
  endforeach()
endif()
if(WIN32)
  target_compile_definitions(cybridge-gain-fixture PRIVATE NOMINMAX UNICODE _UNICODE)
  target_link_libraries(cybridge-gain-fixture PRIVATE user32)
  add_executable(cybridge-gui-tests GuiTests.cpp)
  target_link_libraries(cybridge-gui-tests PRIVATE CyCLAPBridge::CyCLAPBridge user32)
  target_compile_definitions(cybridge-gui-tests PRIVATE NOMINMAX UNICODE _UNICODE)
  add_library(cybridge-lv2-gui-fixture MODULE Lv2GuiFixture.cpp)
  target_include_directories(cybridge-lv2-gui-fixture PRIVATE ../third_party/lv2/include)
  target_compile_definitions(cybridge-lv2-gui-fixture PRIVATE NOMINMAX UNICODE _UNICODE)
  target_compile_features(cybridge-lv2-gui-fixture PRIVATE cxx_std_20)
  target_link_libraries(cybridge-lv2-gui-fixture PRIVATE user32)
  set(_gui_lv2 "${CMAKE_CURRENT_BINARY_DIR}/fixture-gui.lv2")
  file(MAKE_DIRECTORY "${_gui_lv2}")
  set_target_properties(cybridge-lv2-gui-fixture PROPERTIES PREFIX "" OUTPUT_NAME gui-fixture RUNTIME_OUTPUT_DIRECTORY "${_gui_lv2}" LIBRARY_OUTPUT_DIRECTORY "${_gui_lv2}" RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_gui_lv2}" LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_gui_lv2}")
  file(WRITE "${_gui_lv2}/manifest.ttl" "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n<urn:cybridge:test:gui> a lv2:Plugin ; lv2:binary <gui-fixture.dll> ; rdfs:seeAlso <plugin.ttl> .\n")
  file(WRITE "${_gui_lv2}/plugin.ttl" "@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix ui: <http://lv2plug.in/ns/extensions/ui#> .\n@prefix doap: <http://usefulinc.com/ns/doap#> .\n<urn:cybridge:test:gui> a lv2:Plugin ; doap:name \"Original GUI Fixture\" ; ui:ui <urn:cybridge:test:gui#ui> ; lv2:port [ a lv2:InputPort, lv2:AudioPort ; lv2:index 0 ; lv2:symbol \"in_l\" ; lv2:name \"In L\" ], [ a lv2:InputPort, lv2:AudioPort ; lv2:index 1 ; lv2:symbol \"in_r\" ; lv2:name \"In R\" ], [ a lv2:OutputPort, lv2:AudioPort ; lv2:index 2 ; lv2:symbol \"out_l\" ; lv2:name \"Out L\" ], [ a lv2:OutputPort, lv2:AudioPort ; lv2:index 3 ; lv2:symbol \"out_r\" ; lv2:name \"Out R\" ], [ a lv2:InputPort, lv2:ControlPort ; lv2:index 4 ; lv2:symbol \"gain\" ; lv2:name \"Gain\" ; lv2:minimum 0 ; lv2:maximum 2 ; lv2:default 1 ] .\n<urn:cybridge:test:gui#ui> a ui:WindowsUI ; ui:binary <gui-fixture.dll> ; lv2:requiredFeature ui:parent .\n")
  add_test(NAME cybridge-gui-clap COMMAND cybridge-gui-tests clap "$<TARGET_FILE:cybridge-gain-fixture>")
  add_test(NAME cybridge-gui-lv2 COMMAND cybridge-gui-tests lv2 "${_gui_lv2}")
  add_test(NAME cybridge-gui-jsfx COMMAND cybridge-gui-tests jsfx "${CMAKE_CURRENT_SOURCE_DIR}/Gfx.jsfx")
  add_executable(cybridge-vst3-smoke Vst3Smoke.cpp
    ../third_party/vst3sdk/pluginterfaces/base/coreiids.cpp
    ../third_party/vst3sdk/pluginterfaces/base/funknown.cpp
    ../third_party/vst3sdk/public.sdk/source/vst/vstinitiids.cpp)
  target_include_directories(cybridge-vst3-smoke PRIVATE ../third_party/vst3sdk)
  target_compile_features(cybridge-vst3-smoke PRIVATE cxx_std_20)
  if(TARGET cyclapbridge-cli)
    add_test(NAME cybridge-gui-conversion-e2e COMMAND "${CMAKE_COMMAND}"
      "-DCLI=$<TARGET_FILE:cyclapbridge-cli>" "-DCLAP=$<TARGET_FILE:cybridge-gain-fixture>"
      "-DLV2=${_gui_lv2}" "-DJSFX=${CMAKE_CURRENT_SOURCE_DIR}/Gfx.jsfx"
      "-DHOST=$<TARGET_FILE:cybridge-gui-tests>" "-DWORK=${CMAKE_BINARY_DIR}/test-work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/GuiPipeline.cmake")
    set_tests_properties(cybridge-gui-conversion-e2e PROPERTIES TIMEOUT 1800)
    add_test(NAME cybridge-conversion-e2e COMMAND "${CMAKE_COMMAND}"
      "-DCLI=$<TARGET_FILE:cyclapbridge-cli>"
      "-DFIXTURE=$<TARGET_FILE:cybridge-gain-fixture>"
      "-DSMOKE=$<TARGET_FILE:cybridge-vst3-smoke>"
      "-DWORK=${CMAKE_BINARY_DIR}/test-work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/EndToEnd.cmake")
    set_tests_properties(cybridge-conversion-e2e PROPERTIES TIMEOUT 300)
    add_test(NAME cybridge-multiformat-e2e COMMAND "${CMAKE_COMMAND}"
      "-DCLI=$<TARGET_FILE:cyclapbridge-cli>"
      "-DFIXTURE=$<TARGET_FILE:cybridge-gain-fixture>"
      "-DJSFX=${CMAKE_CURRENT_SOURCE_DIR}/Gain.jsfx"
      "-DHOST=$<TARGET_FILE:cybridge-multi-hosts>"
      "-DWORK=${CMAKE_BINARY_DIR}/test-work"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/MultiPipeline.cmake")
    set_tests_properties(cybridge-multiformat-e2e PROPERTIES TIMEOUT 1200)
  endif()
endif()
