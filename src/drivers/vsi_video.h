#ifndef VSI_VIDEO_H
#define VSI_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

// VSI Video 配置
#define VSI_VIDEO_WIDTH  640
#define VSI_VIDEO_HEIGHT 480
#define VSI_VIDEO_CHANNELS 3

// VSI Video 控制器
class VSIVideoController {
public:
    VSIVideoController(const char* video_path);
    ~VSIVideoController();
    
    // 初始化 VSI 視訊源
    bool init();
    
    // 讀取下一幀
    bool getNextFrame(uint8_t* frame_buffer);
    
    // 獲取當前幀號
    int getCurrentFrameNumber() const { return frame_count_; }
    
    // 獲取總幀數
    int getTotalFrames() const { return total_frames_; }
    
    // 重置到第一幀
    void reset();
    
    // 檢查是否還有幀
    bool hasMoreFrames() const;
    
private:
    const char* video_path_;
    int frame_count_;
    int total_frames_;
    uint8_t* frame_buffer_;
    bool initialized_;
    
    // VSI 相關
    void* vsi_handle_;
    
    // 從影片檔案讀取幀
    bool readFrameFromFile();
};

// VSI Video 輸出控制器
class VSIVideoOutput {
public:
    VSIVideoOutput();
    ~VSIVideoOutput();

    // 初始化 VSI 視訊輸出
    bool init();

    // 發送幀
    bool sendFrame(const uint8_t* frame_buffer);

private:
    bool initialized_;
    uint8_t* output_buffer_;
};

#endif // VSI_VIDEO_H