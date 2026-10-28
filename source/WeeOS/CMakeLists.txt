add_library(WeeOSApplication STATIC
    Application.cpp
    Console.cpp
)
target_link_libraries(WeeOSApplication PUBLIC Boot Graphics Input Ios Platform SystemMenu)

add_executable(WeeOS main.cpp)
target_link_libraries(WeeOS PRIVATE WeeOSApplication)
CompileGraphicsShaders(WeeOS)
