add_library(Formats STATIC
    Aes128.cpp
    Ash0.cpp
    BannerContent.cpp
    BigNumber.cpp
    Boot2Image.cpp
    BootMiiKeyBlock.cpp
    ByteReader.cpp
    ByteWriter.cpp
    BoardMessageFile.cpp
    CdbRecordFile.cpp
    CdbSdCopy.cpp
    Certificate.cpp
    Crc32.cpp
    ConsoleKeys.cpp
    DeviceCertificate.cpp
    DolExecutable.cpp
    EffectFile.cpp
    EllipticCurve.cpp
    FileBytes.cpp
    Font.cpp
    GifImage.cpp
    HexText.cpp
    HmacSha1.cpp
    Huffman.cpp
    Inflate.cpp
    Layout.cpp
    LayoutAnimation.cpp
    LayoutMaterial.cpp
    Lz77.cpp
    Md5.cpp
    MessageFile.cpp
    NandBootInfoFile.cpp
    NandFileSystem.cpp
    NandImage.cpp
    NandTitles.cpp
    NumberedMessageFile.cpp
    OneTimeProgrammable.cpp
    PlayLogAttachment.cpp
    PlayRecordFile.cpp
    PngImage.cpp
    PostedPictureFile.cpp
    ResourceFile.cpp
    RgbaImage.cpp
    RsaSignature.cpp
    SaveBanner.cpp
    SdCardBackup.cpp
    SectionedFile.cpp
    SerialEeprom.cpp
    SettingTextFile.cpp
    Sha1.cpp
    Sha256.cpp
    Signature.cpp
    SignatureVerification.cpp
    StateFile.cpp
    SystemConfiguration.cpp
    TextureDecoder.cpp
    TextureFormat.cpp
    Ticket.cpp
    TitleEncryption.cpp
    TitleMetadata.cpp
    TitleMetadataBackupFile.cpp
    TplTexture.cpp
    U8Archive.cpp
    Vectors.cpp
    VirtualFatVolume.cpp
    WadFile.cpp
    Yaz0.cpp
    ZipArchive.cpp
)

# Reading the NAND decrypts and hashes hundreds of megabytes, and finding the keys of a console hashes every 16 bytes of its programs.
# Without optimization these files take most of the time of the tests of the dump, so they are optimized in every configuration.
# The runtime checks of the debug build cannot be combined with that. /Od and /Ob0 are taken out so that /O2 neither overrides /Od with
# a warning nor stays without inlining; the other files are not optimized either way.
if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set_source_files_properties(Aes128.cpp Sha1.cpp Sha256.cpp PROPERTIES COMPILE_OPTIONS "/O2")
endif()
