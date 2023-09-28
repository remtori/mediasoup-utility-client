#pragma once

#include "msc/msc.hpp"

#include <api/create_peerconnection_factory.h>
#include <rtc_base/thread.h>

namespace msc
{

std::shared_ptr<PeerConnectionFactoryTuple> default_peer_connection_factory();

class PeerConnectionFactoryTupleImpl : public PeerConnectionFactoryTuple
{
public:
    PeerConnectionFactoryTupleImpl();
    ~PeerConnectionFactoryTupleImpl() override;

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory()
    {
        return m_peer_connection_factory;
    }
private:
    std::unique_ptr<rtc::Thread> m_network_thread;
    std::unique_ptr<rtc::Thread> m_signaling_thread;
    std::unique_ptr<rtc::Thread> m_worker_thread;
    rtc::scoped_refptr<webrtc::AudioDeviceModule> m_adm;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_peer_connection_factory;
};

}