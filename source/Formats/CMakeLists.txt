add_library(Formats STATIC
    Aes128.cpp
    Ash0.cpp
    BigNumber.cpp
    Boot2Image.cpp
    BootMiiKeyBlock.cpp
    ByteReader.cpp
    ByteWriter.cpp
    Certificate.cpp
    ConsoleKeys.cpp
    DeviceCertificate.cpp
    DolExecutable.cpp
    EllipticCurve.cpp
    FileBytes.cpp
    Font.cpp
    HexText.cpp
    HmacSha1.cpp
    Huffman.cpp
    Layout.cpp
    LayoutAnimation.cpp
    LayoutMaterial.cpp
    Lz77.cpp
    MessageFile.cpp
    NandBootInfoFile.cpp
    NandFileSystem.cpp
    NandImage.cpp
    NandTitles.cpp
    OneTimeProgrammable.cpp
    RgbaImage.cpp
    RsaSignature.cpp
    SectionedFile.cpp
    SerialEeprom.cpp
    SettingTextFile.cpp
    Sha1.cpp
    Signature.cpp
    SignatureVerification.cpp
    StateFile.cpp
    SystemConfiguration.cpp
    TextureDecoder.cpp
    TextureFormat.cpp
    Ticket.cpp
    TitleEncryption.cpp
    TitleMetadata.cpp
    TplTexture.cpp
    U8Archive.cpp
    Vectors.cpp
    WadFile.cpp
    Yaz0.cpp
)

# Reading the NAND decrypts and hashes hundreds of megabytes. Without optimization these two files take most of the time of the tests of
# the dump, so they are optimized in every configuration. The runtime checks of the debug build cannot be combined with that.
if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set_source_files_properties(Aes128.cpp Sha1.cpp PROPERTIES COMPILE_OPTIONS "/O2")
endif()
