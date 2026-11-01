add_library(Settings STATIC
    ConsoleClock.cpp
    Countries.cpp
    ParentalMasterKey.cpp
    PcKeys.cpp
    PcSettings.cpp
    SystemMemoryFormatter.cpp
    SystemSettings.cpp
)

target_link_libraries(Settings PUBLIC Formats Ios Input)
