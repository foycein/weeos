add_library(Sound STATIC
    BankFile.cpp
    NoteParameters.cpp
    Reference.cpp
    Resampler.cpp
    SequenceFile.cpp
    SoundArchiveFile.cpp
    SoundFileHeader.cpp
    WaveDecoder.cpp
    WaveFileWriter.cpp
    WaveInfo.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
