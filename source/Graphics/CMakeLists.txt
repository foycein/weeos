add_library(Graphics STATIC
    CanvasMapping.cpp
    DrawData.cpp
    FramePacer.cpp
    MaterialCheck.cpp
    StageList.cpp
)

target_link_libraries(Graphics PUBLIC Formats Platform)
