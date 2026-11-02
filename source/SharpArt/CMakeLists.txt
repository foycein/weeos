add_library(SharpArt STATIC
    ClockPictures.cpp
    HomeMenuPictures.cpp
    Paint.cpp
    SharpPictures.cpp
    VectorCanvas.cpp
    VectorPath.cpp
    WiiLogoPictures.cpp
    WiiMenuPictures.cpp
    WiiOptionsPictures.cpp
)

target_link_libraries(SharpArt PUBLIC Formats)
