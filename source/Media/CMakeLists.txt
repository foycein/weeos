add_library(Media STATIC
    AacMusic.cpp
    AviReader.cpp
    ExifReader.cpp
    InverseDct.cpp
    JpegBitReader.cpp
    JpegDecoder.cpp
    JpegFrame.cpp
    JpegHuffmanTable.cpp
    JpegPicture.cpp
    JpegScanDecoder.cpp
    MovieFrames.cpp
    MovieSound.cpp
    PictureShapes.cpp
    QuickTimeReader.cpp
)

target_link_libraries(Media PUBLIC Formats)

# Windows' own AAC decoder, allowed by the owner for music files (docs/DECISIONS.md).
target_link_libraries(Media PRIVATE mfplat mfreadwrite mfuuid shlwapi ole32)
