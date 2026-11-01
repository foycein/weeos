# weeossetup.exe, the installer of the release. It carries the portable zip of WeeOS, so it is only built when
# tools/Release/MakeRelease.py names that zip.
set(WeeOSSetupPackage "" CACHE FILEPATH "The portable zip of WeeOS that weeossetup.exe carries (tools/Release/MakeRelease.py)")
if(NOT WeeOSSetupPackage)
    return()
endif()

set(WeeOSIcon ${CMAKE_SOURCE_DIR}/source/WeeOS/WeeOS.ico)
set(SetupResources ${CMAKE_CURRENT_BINARY_DIR}/Setup.rc)
configure_file(Setup.rc.in ${SetupResources})
set_source_files_properties(${SetupResources} PROPERTIES OBJECT_DEPENDS "${WeeOSSetupPackage};${WeeOSIcon}")

add_executable(weeossetup WIN32
    main.cpp
    Installation.cpp
    Package.cpp
    SetupWindow.cpp
    Setup.manifest
    ${SetupResources}
)
target_link_libraries(weeossetup PRIVATE Formats Msimg32 Ole32 Shell32 Uuid)
target_compile_definitions(weeossetup PRIVATE UNICODE _UNICODE NOMINMAX)
