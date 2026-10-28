add_library(Sound STATIC
    BankFile.cpp
    NoteParameters.cpp
    Reference.cpp
    SequenceFile.cpp
    SoundArchiveFile.cpp
    SoundFileHeader.cpp
    WaveDecoder.cpp
    WaveInfo.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
