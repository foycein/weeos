add_library(Media STATIC
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
