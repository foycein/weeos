add_library(Boot STATIC
    BootSelection.cpp
)
target_link_libraries(Boot PUBLIC Formats)
