add_library(Mii STATIC
    DefaultMiis.cpp
    MiiChecksum.cpp
    MiiCreateId.cpp
    MiiDatabase.cpp
    MiiRecord.cpp
    MiiResources.cpp
    RandomMii.cpp
    RandomMiiTables.cpp
)

target_link_libraries(Mii PUBLIC Formats)
