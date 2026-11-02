add_library(Media STATIC
    AacMusic.cpp
    AviReader.cpp
    ExifReader.cpp
    InverseDct.cpp
    JpegBitReader.cpp
    JpegDecoder.cpp
    JpegEncoder.cpp
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

# Windows' own AAC decoder, only for music files.
target_link_libraries(Media PRIVATE mfplat mfreadwrite mfuuid shlwapi ole32)
