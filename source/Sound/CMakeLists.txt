add_library(Sound STATIC
    AudioOutput.cpp
    BankFile.cpp
    Envelope.cpp
    LoadedFiles.cpp
    LowPassFilter.cpp
    Mixer.cpp
    Modulation.cpp
    NoteParameters.cpp
    Reference.cpp
    Resampler.cpp
    Reverb.cpp
    SequenceFile.cpp
    SequenceNotes.cpp
    SequencePlayer.cpp
    SoundArchiveFile.cpp
    SoundCommandQueue.cpp
    SoundFileHeader.cpp
    SoundMode.cpp
    SoundSystem.cpp
    SpeakerSounds.cpp
    StereoPlacement.cpp
    Voice.cpp
    VoicePool.cpp
    WaveArchiveFile.cpp
    WaveDecoder.cpp
    WaveFileWriter.cpp
    WaveInfo.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
target_link_libraries(Sound PRIVATE ole32)
