add_library(Sound STATIC
    AudioCapture.cpp
    AudioOutput.cpp
    BankFile.cpp
    BannerSound.cpp
    Envelope.cpp
    LoadedFiles.cpp
    LowPassFilter.cpp
    Mixer.cpp
    Modulation.cpp
    MonoStreamConverter.cpp
    NoteParameters.cpp
    PlaybackDevices.cpp
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
    StreamFile.cpp
    Voice.cpp
    VoicePool.cpp
    WaveArchiveFile.cpp
    WaveDecoder.cpp
    WaveFileWriter.cpp
    WaveInfo.cpp
    WavePlayer.cpp
    WaveSoundFile.cpp
)
target_link_libraries(Sound PUBLIC Formats)
target_link_libraries(Sound PRIVATE ole32)

# The tests mix every sound of the menu and the channels into buffers. Without optimization the mixing takes minutes,
# so the files that run once per sample are optimized in every configuration. The runtime checks of the debug build cannot be combined with that.
if(MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set_source_files_properties(Envelope.cpp LowPassFilter.cpp Mixer.cpp MonoStreamConverter.cpp Resampler.cpp Reverb.cpp Voice.cpp
        WaveDecoder.cpp WavePlayer.cpp
        PROPERTIES COMPILE_OPTIONS "/O2")
endif()
