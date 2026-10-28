add_library(Sound STATIC
    BankFile.cpp
    Envelope.cpp
    LoadedFiles.cpp
    Mixer.cpp
    NoteParameters.cpp
    Reference.cpp
    Resampler.cpp
    SequenceFile.cpp
    SoundArchiveFile.cpp
    SoundCommandQueue.cpp
    SoundFileHeader.cpp
    SoundSystem.cpp
    Voice.cpp
    VoicePool.cpp
    WaveDecoder.cpp
    WaveFileWriter.cpp
    WaveInfo.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
