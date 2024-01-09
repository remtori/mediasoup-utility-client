#include "./video-writer.hpp"

#include <fmt/format.h>

VideoWriter::VideoWriter(std::string filepath)
    : m_filepath(std::move(filepath))
{
}

VideoWriter::~VideoWriter()
{
}

void VideoWriter::init_writer(int width, int height, int fps)
{
    m_video_writer = std::make_unique<cv::VideoWriter>();
    m_video_writer->open("output/video.mp4", cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, cv::Size(width, height), true);
    if (!m_video_writer->isOpened()) {
        fmt::println("Error: could not open video writer");
        return;
    }

    fmt::println("setting up finish ok");
}

void VideoWriter::on_close()
{
    if (m_video_writer)
    {
        m_video_writer->release();
        m_video_writer.reset();
    }
}

void VideoWriter::on_video_frame(const msc::VideoFrame& captured_frame)
{
    if (m_frame_count == 1)
        init_writer(captured_frame.width, captured_frame.height, 30);
    m_frame_count++;

    cv::Mat y_plane(captured_frame.height, captured_frame.width, CV_8UC1, const_cast<uint8_t*>(captured_frame.data_y));
    cv::Mat u_plane(captured_frame.height / 2, captured_frame.width / 2, CV_8UC1, const_cast<uint8_t*>(captured_frame.data_u));
    cv::Mat v_plane(captured_frame.height / 2, captured_frame.width / 2, CV_8UC1, const_cast<uint8_t*>(captured_frame.data_v));

    cv::Mat i420_frame;
    cv::merge(std::vector<cv::Mat>{y_plane, u_plane, v_plane}, i420_frame);

    m_video_writer->write(i420_frame);
}
