add_library(NandImporter STATIC
    DumpVerification.cpp
    FolderFiles.cpp
    IdentityImporter.cpp
    ImportRules.cpp
    NandImporter.cpp
    PersonalSettings.cpp
    PreparedTitles.cpp
    WindowsNames.cpp
)
target_link_libraries(NandImporter PUBLIC Formats Ios)
target_include_directories(NandImporter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(NandTool main.cpp)
target_link_libraries(NandTool PRIVATE Formats NandImporter)
