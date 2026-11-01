add_library(Media STATIC
    ExifReader.cpp
    InverseDct.cpp
    JpegBitReader.cpp
    JpegDecoder.cpp
    JpegFrame.cpp
    JpegHuffmanTable.cpp
    JpegPicture.cpp
    JpegScanDecoder.cpp
    PictureShapes.cpp
)

target_link_libraries(Media PUBLIC Formats)
