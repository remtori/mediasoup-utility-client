#pragma once

#include "msc/msc.hpp"
#include <mediasoupclient.hpp>

namespace msc {

class SinkImpl {
public:
    virtual ~SinkImpl() = default;

    virtual bool is_consumer_equal(const mediasoupclient::Consumer*) = 0;
    virtual bool is_user_ptr_equal(const void*) = 0;
    virtual void on_close() = 0;
};

class AudioSinkImpl : public SinkImpl
    , public webrtc::AudioTrackSinkInterface {
public:
    AudioSinkImpl(std::unique_ptr<mediasoupclient::Consumer> consumer, std::shared_ptr<AudioConsumer> user_consumer)
        : m_consumer(std::move(consumer))
        , m_user_consumer(std::move(user_consumer))
    {
        if (m_user_consumer)
            dynamic_cast<webrtc::AudioTrackInterface*>(m_consumer->GetTrack())->AddSink(this);
    }

    ~AudioSinkImpl() override
    {
        if (m_user_consumer)
            dynamic_cast<webrtc::AudioTrackInterface*>(m_consumer->GetTrack())->RemoveSink(this);

        m_consumer->Close();
    }

    void OnData(
        const void* data,
        int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames,
        absl::optional<int64_t> absolute_capture_timestamp_ms) override;

    bool is_consumer_equal(const mediasoupclient::Consumer* consumer) override
    {
        return consumer == m_consumer.get();
    }

    bool is_user_ptr_equal(const void* other) override
    {
        return other == m_user_consumer.get();
    }

    void on_close() override
    {
        if (m_user_consumer)
            m_user_consumer->on_close();
    }

private:
    std::unique_ptr<mediasoupclient::Consumer> m_consumer;
    std::shared_ptr<AudioConsumer> m_user_consumer;
};

class VideoSinkImpl : public SinkImpl
    , public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    VideoSinkImpl(std::unique_ptr<mediasoupclient::Consumer> consumer, std::shared_ptr<VideoConsumer> user_consumer)
        : m_consumer(std::move(consumer))
        , m_user_consumer(std::move(user_consumer))
    {
        if (m_user_consumer)
            dynamic_cast<webrtc::VideoTrackInterface*>(m_consumer->GetTrack())->AddOrUpdateSink(this, {});
    }

    ~VideoSinkImpl() override
    {
        if (m_user_consumer)
            dynamic_cast<webrtc::VideoTrackInterface*>(m_consumer->GetTrack())->RemoveSink(this);

        m_consumer->Close();
    }

    void OnFrame(const webrtc::VideoFrame& frame) override;

    bool is_consumer_equal(const mediasoupclient::Consumer* consumer) override
    {
        return consumer == m_consumer.get();
    }

    bool is_user_ptr_equal(const void* other) override
    {
        return other == m_user_consumer.get();
    }

    void on_close() override
    {
        if (m_user_consumer)
            m_user_consumer->on_close();
    }

private:
    std::unique_ptr<mediasoupclient::Consumer> m_consumer;
    std::shared_ptr<VideoConsumer> m_user_consumer;
};

class DataConsumerImpl : public mediasoupclient::DataConsumer::Listener {
public:
    DataConsumerImpl(std::shared_ptr<DataConsumer> user_consumer)
        : m_user_consumer(std::move(user_consumer))
    {
    }

    virtual ~DataConsumerImpl()
    {
        if (m_consumer) {
            m_consumer->Close();
        }
    }

    bool is_consumer_equal(const mediasoupclient::DataConsumer* consumer)
    {
        return consumer == m_consumer.get();
    }

    bool is_user_ptr_equal(const void* ptr)
    {
        return ptr == m_user_consumer.get();
    }

    void set_consumer(std::unique_ptr<mediasoupclient::DataConsumer> consumer)
    {
        m_consumer = std::move(consumer);
    }

private:
    void OnConnecting(mediasoupclient::DataConsumer*) override { }
    void OnOpen(mediasoupclient::DataConsumer*) override { }
    void OnClosing(mediasoupclient::DataConsumer*) override { }
    void OnClose(mediasoupclient::DataConsumer*) override { }
    void OnTransportClose(mediasoupclient::DataConsumer*) override { }

    void OnMessage(mediasoupclient::DataConsumer*, const webrtc::DataBuffer& buffer) override
    {
        if (m_user_consumer) {
            m_user_consumer->on_data(std::span(buffer.data.data(), buffer.size()));
        }
    }

private:
    std::unique_ptr<mediasoupclient::DataConsumer> m_consumer { nullptr };
    std::shared_ptr<DataConsumer> m_user_consumer;
};

}
