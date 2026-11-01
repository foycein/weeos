#include "WeeOS/PcMicrophone.h"

#include "Ios/Microphone/MicrophoneService.h"

#include <iostream>

namespace WeeOS
{

bool PcMicrophone::Start(Ios::MicrophoneDelivery delivery)
{
    Stop();
    try
    {
        capture = std::make_unique<Sound::AudioCapture>(Ios::MicrophoneSampleRate, std::move(delivery));
    }
    catch (const Sound::AudioCaptureError& error)
    {
        std::cout << "The microphone does not open: " << error.what() << "\n";
        return false;
    }

    ++openedSessions;
    return true;
}

void PcMicrophone::Stop()
{
    if (!capture)
        return;

    capture.reset();
    ++closedSessions;
}

int PcMicrophone::OpenedSessionCount() const
{
    return openedSessions;
}

int PcMicrophone::ClosedSessionCount() const
{
    return closedSessions;
}

}
