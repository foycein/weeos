add_library(Mii STATIC
    DefaultMiis.cpp
    MiiBody.cpp
    MiiBodyFile.cpp
    MiiBodyPicture.cpp
    MiiChecksum.cpp
    MiiColors.cpp
    MiiCreateId.cpp
    MiiDatabase.cpp
    MiiExpression.cpp
    MiiFaceLayout.cpp
    MiiFaceTexture.cpp
    MiiHead.cpp
    MiiHeadModel.cpp
    MiiIcon.cpp
    MiiRecord.cpp
    MiiResources.cpp
    MiiTextureUpload.cpp
    RandomMii.cpp
    RandomMiiTables.cpp
)

target_link_libraries(Mii PUBLIC Formats Graphics)
