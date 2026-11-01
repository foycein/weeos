add_library(WeeOSApplication STATIC
    Application.cpp
    PcMicrophone.cpp
    ProgramSound.cpp
    WiiSettingsServices.cpp
)
target_link_libraries(WeeOSApplication PUBLIC Boot Channels Graphics Input Ios Platform Settings Sound SystemMenu)

add_executable(WeeOS main.cpp)
target_link_libraries(WeeOS PRIVATE WeeOSApplication)
CompileGraphicsShaders(WeeOS)
