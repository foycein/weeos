add_library(WeeOSApplication STATIC
    Application.cpp
    ConsoleImporter.cpp
    NewConsole.cpp
    ProgramSound.cpp
    WiiSettingsServices.cpp
)
target_link_libraries(WeeOSApplication PUBLIC Boot Channels Graphics Input Ios NandImporter Platform Settings Sound SystemMenu)

add_executable(WeeOS main.cpp WeeOS.rc)
target_link_libraries(WeeOS PRIVATE WeeOSApplication)
CompileGraphicsShaders(WeeOS)

# The Wii files beside WeeOS.exe, from which the first start makes a new console (tools/PrepareWiiFiles).
set(WiiFilesFolder ${CMAKE_CURRENT_BINARY_DIR}/WiiFiles CACHE INTERNAL "The prepared Wii files beside WeeOS.exe")
add_custom_command(
    OUTPUT ${WiiFilesFolder}/titles.txt
    COMMAND PrepareWiiFiles ${WiiFilesFolder}
    DEPENDS PrepareWiiFiles
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Preparing the Wii files beside WeeOS.exe"
)
add_custom_target(WiiFiles ALL DEPENDS ${WiiFilesFolder}/titles.txt)
add_dependencies(WeeOS WiiFiles)
