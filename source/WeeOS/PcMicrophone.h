#pragma once

#include "Ios/Microphone/MicrophoneSource.h"
#include "Sound/AudioCapture.h"

#include <atomic>
#include <memory>

namespace WeeOS
{

// The Wii Speak accessory of WeeOS.exe: the default microphone of Windows. A capture session opens only while a channel has the
// microphone open, and closes with it, so Windows shows the microphone in use only then. What it hears goes to the microphone service
// of IOS and is not kept anywhere else.
class PcMicrophone : public Ios::MicrophoneSource
{
public:
    // False when the capture device does not open; the service then says the accessory has a problem.
    bool Start(Ios::MicrophoneDelivery delivery) override;
    void Stop() override;

    // How many capture sessions were opened and closed since the program started.
    int OpenedSessionCount() const;
    int ClosedSessionCount() const;

private:
    std::unique_ptr<Sound::AudioCapture> capture;
    std::atomic<int> openedSessions{0};
    std::atomic<int> closedSessions{0};
};

}
