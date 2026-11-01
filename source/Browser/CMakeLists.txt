# Chromium through WebView2 (external/Chromium): the headers and the loader of Microsoft's SDK. The engine itself is the WebView2 Runtime
# of Windows 11.
set(WEBVIEW2_FOLDER "${CMAKE_SOURCE_DIR}/external/Chromium")

add_library(Browser STATIC
    Addresses.cpp
    DevToolsMessages.cpp
    PageCapture.cpp
    PageEngine.cpp
    PageThread.cpp
    TextConversion.cpp
    WebPage.cpp
)

target_include_directories(Browser PRIVATE "${WEBVIEW2_FOLDER}/include")
# WebView2.h needs the COM headers that WIN32_LEAN_AND_MEAN would leave out.
target_compile_definitions(Browser PRIVATE NOMINMAX UNICODE _UNICODE)
target_link_libraries(Browser PUBLIC Formats PRIVATE "${WEBVIEW2_FOLDER}/x64/WebView2Loader.dll.lib" d3d11 dxgi shlwapi windowsapp delayimp)

# The loader is only loaded when a program opens its first page, so programs that never browse do not need the DLL beside them.
target_link_options(Browser INTERFACE "/DELAYLOAD:WebView2Loader.dll")

# A program that opens pages gets the loader beside it.
function(CopyWebViewLoader targetName)
    add_custom_command(TARGET ${targetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/external/Chromium/x64/WebView2Loader.dll" "$<TARGET_FILE_DIR:${targetName}>")
endfunction()

# The capture copies every picture of the page pixel by pixel, sixty times a second, so it is optimized in every configuration. The
# runtime checks of the debug build cannot be combined with that.
if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set_source_files_properties(PageCapture.cpp PROPERTIES COMPILE_OPTIONS "/O2")
endif()
