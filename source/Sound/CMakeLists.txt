add_library(Sound STATIC
    AudioOutput.cpp
    BankFile.cpp
    Envelope.cpp
    LoadedFiles.cpp
    Mixer.cpp
    NoteParameters.cpp
    PlaybackDevices.cpp
    Reference.cpp
    Resampler.cpp
    SequenceFile.cpp
    SequencePlayer.cpp
    SoundArchiveFile.cpp
    SoundCommandQueue.cpp
    SoundFileHeader.cpp
    SoundSystem.cpp
    StereoPlacement.cpp
    Voice.cpp
    VoicePool.cpp
    WaveDecoder.cpp
    WaveFileWriter.cpp
    WaveInfo.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
target_link_libraries(Sound PRIVATE ole32)
