#include <iostream>

#include <mediasoupclient.hpp>

#include <system_wrappers/include/clock.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>

void createPeerConnectionFactory()
{
    auto networkThread   = rtc::Thread::Create();
    auto signalingThread = rtc::Thread::Create();
    auto workerThread    = rtc::Thread::Create();

    networkThread->SetName("network_thread", nullptr);
    signalingThread->SetName("signaling_thread", nullptr);
    workerThread->SetName("worker_thread", nullptr);

    if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
    {
        std::cout << "thread start errored" << std::endl;
    }

    webrtc::PeerConnectionInterface::RTCConfiguration config;

    auto factory = webrtc::CreatePeerConnectionFactory(
            networkThread.get(),
            workerThread.get(),
            signalingThread.get(),
            nullptr,
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            webrtc::CreateBuiltinVideoEncoderFactory(),
            webrtc::CreateBuiltinVideoDecoderFactory(),
            nullptr /*audio_mixer*/,
            nullptr /*audio_processing*/);


}

int main()
{
    auto logLevel = mediasoupclient::Logger::LogLevel::LOG_DEBUG;
    mediasoupclient::Logger::SetLogLevel(logLevel);
    mediasoupclient::Logger::SetDefaultHandler();

    // Initilize mediasoupclient.
    mediasoupclient::Initialize();

    createPeerConnectionFactory();

    return 0;
}