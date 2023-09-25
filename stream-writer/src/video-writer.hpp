#include <memory>
#include <string>

#include <msc/msc.hpp>
#include <opencv2/opencv.hpp>

class VideoWriter : public msc::VideoConsumer {
public:
    VideoWriter(std::string filepath);
    ~VideoWriter();

    void on_video_frame(const msc::VideoFrame&) override;
    void on_close() override;

private:
    void init_writer(int width, int height, int fps);

private:
    std::string m_filepath;
    int64_t m_frame_count { 1 };

    std::unique_ptr<cv::VideoWriter> m_video_writer { nullptr };
};
