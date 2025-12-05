#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsi_video.h"
#include "yolo_pose.h"
#include "reid.h"
#include "image_utils.h"
#include "draw_utils.h"
#include "lcd_display.h"
#include <ethosu_driver.h>
#include "CMSIS_5/Device/ARM/ARMCM55/Include/ARMCM55.h"

extern "C" void initialise_monitor_handles(void);

// Ethos-U55 Base Address on Corstone-300
#define ETHOSU_BASE_ADDRESS 0x48102000
#define ETHOSU_IRQ 56

// Global driver instance
struct ethosu_driver ethosu_drv;

extern "C" void Interrupt56_Handler(void) {
    ethosu_irq_handler(&ethosu_drv);
}

void ethosu_init_driver() {
    printf("Initializing Ethos-U driver at 0x%08X...\n", ETHOSU_BASE_ADDRESS);

    if (ethosu_init(&ethosu_drv, (void*)ETHOSU_BASE_ADDRESS, NULL, 0, 1, 1) != 0) {
        printf("Failed to initialize Ethos-U driver\n");
        return;
    }

    printf("Ethos-U driver initialized\n");
    
    NVIC_EnableIRQ((IRQn_Type)ETHOSU_IRQ);
}

// 模型資料宣告 (由 tflite_to_cc.py 生成的 .cc 檔案提供定義)
extern "C" {
    extern const unsigned char yolo_model_data[];
    extern const unsigned int yolo_model_data_len;
    extern const unsigned char reid_model_data[];
    extern const unsigned int reid_model_data_len;
}

// 全域物件
static YoloPoseDetector* yolo_detector = nullptr;
static ReIDMatcher* reid_matcher = nullptr;
static VSIVideoController* video_controller = nullptr;
static LCDDisplay* lcd_display = nullptr;

// 處理單幀並繪製結果
void processFrame(uint8_t* frame, int frame_number) {
    printf("\n========== Frame %d ==========\n", frame_number);

    // Step 1: YOLO 偵測人物
    auto detections = yolo_detector->detect(frame, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT);
    
    if (detections.empty()) {
        printf("No persons detected\n");
        return;
    }
    
    // 計算縮放比例 (YOLO 輸入到原始影像)
    float scale_x = (float)VSI_VIDEO_WIDTH / YOLO_INPUT_WIDTH;
    float scale_y = (float)VSI_VIDEO_HEIGHT / YOLO_INPUT_HEIGHT;
    
    // 創建繪圖用的幀副本
    uint8_t* display_frame = nullptr;
    if (lcd_display) {
        display_frame = new uint8_t[VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * 3];
        memcpy(display_frame, frame, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * 3);
    }
    
    // Step 2: 對每個偵測到的人進行 Re-ID
    for (size_t i = 0; i < detections.size(); i++) {
        printf("\n--- Person %zu/%zu ---\n", i + 1, detections.size());
        printf("BBox: (%.1f, %.1f, %.1f, %.1f), Conf: %.3f\n",
               detections[i].bbox.x, detections[i].bbox.y,
               detections[i].bbox.w, detections[i].bbox.h,
               detections[i].confidence);
        
        // bbox 已經是像素座標了
        int x1 = (int)detections[i].bbox.x;
        int y1 = (int)detections[i].bbox.y;
        int x2 = (int)(detections[i].bbox.x + detections[i].bbox.w);
        int y2 = (int)(detections[i].bbox.y + detections[i].bbox.h);
        
        // 將座標從 YOLO 輸入尺寸映射到原始影像尺寸
        x1 = (int)(x1 * scale_x);
        y1 = (int)(y1 * scale_y);
        x2 = (int)(x2 * scale_x);
        y2 = (int)(y2 * scale_y);
        
        // 邊界檢查
        x1 = (x1 < 0) ? 0 : x1;
        y1 = (y1 < 0) ? 0 : y1;
        x2 = (x2 >= VSI_VIDEO_WIDTH) ? VSI_VIDEO_WIDTH - 1 : x2;
        y2 = (y2 >= VSI_VIDEO_HEIGHT) ? VSI_VIDEO_HEIGHT - 1 : y2;
        
        int crop_w = x2 - x1;
        int crop_h = y2 - y1;
        
        if (crop_w < 20 || crop_h < 40) {
            printf("Person too small, skipping Re-ID\n");
            continue;
        }
        
        // 裁切人物區域
        uint8_t* cropped = new uint8_t[crop_w * crop_h * 3];
        ImageUtils::crop(frame, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT,
                        cropped, x1, y1, crop_w, crop_h);
        
        // Re-ID 特徵提取
        float features[REID_FEATURE_DIM];
        if (reid_matcher->extractFeatures(cropped, crop_w, crop_h, features)) {
            // 匹配或加入 Gallery
            int person_id = reid_matcher->matchInGallery(features, frame_number);
            
            if (person_id < 0) {
                person_id = reid_matcher->addToGallery(features, frame_number);
            }
            
            printf(">>> FINAL RESULT: Person ID = %d <<<\n", person_id);
            
            // Print ReID Vector (First 10 elements)
            printf("ReID Vector (first 10/512): [");
            for(int v=0; v<10; v++) printf("%.4f ", features[v]);
            printf("...]\n");

            // 繪製偵測結果到顯示幀
            if (lcd_display && display_frame) {
                DrawUtils::drawDetection(display_frame, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT,
                                        detections[i], person_id, scale_x, scale_y);
            }
            
            // 輸出骨架關鍵點
            printf("Pose Keypoints:\n");
            const char* kpt_names[] = {"Nose", "LEye", "REye", "LEar", "REar", "LShldr", "RShldr", "LElbow", "RElbow", "LWrist", "RWrist", "LHip", "RHip", "LKnee", "RKnee", "LAnkle", "RAnkle"};
            int visible_keypoints = 0;
            for (int k = 0; k < NUM_KEYPOINTS; k++) {
                if (detections[i].keypoints[k].score > 0.5f) {
                    visible_keypoints++;
                    printf("  %-6s: (%3d, %3d) score=%.2f\n", kpt_names[k], 
                           detections[i].keypoints[k].x, detections[i].keypoints[k].y, 
                           detections[i].keypoints[k].score);
                }
            }
            printf("Keypoints: %d/%d visible\n", visible_keypoints, NUM_KEYPOINTS);
        }
        
        delete[] cropped;
    }
    
    // 顯示帶有標註的幀到 LCD
    if (lcd_display && display_frame) {
        lcd_display->displayFrame(display_frame, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT);
    }
    
    if (display_frame) {
        delete[] display_frame;
    }
}

int main(int argc, char* argv[]) {
    initialise_monitor_handles();
    // setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering
    setvbuf(stdout, NULL, _IOLBF, 1024); // Enable line buffering
    printf("Application started.\n");
    ethosu_init_driver();

    printf("\n");
    printf("========================================\n");
    printf(" YOLOv8-Pose + Re-ID on ARM FVP\n");
    printf(" Corstone-300 + Ethos-U55\n");
    printf("========================================\n\n");
    
    // 檢查參數
    const char* video_path = "test_videos/illit_dance_short.mp4";
    if (argc > 1) {
        video_path = argv[1];
    }
    
    printf("Video input: %s\n\n", video_path);
    
    // 初始化 VSI 視訊控制器
    video_controller = new VSIVideoController(video_path);
    if (!video_controller->init()) {
        printf("Failed to initialize video controller\n");
        return -1;
    }
    
    // 初始化 YOLO
    yolo_detector = new YoloPoseDetector();
    if (!yolo_detector->init(yolo_model_data, yolo_model_data_len)) {
        printf("Failed to initialize YOLO detector\n");
        return -1;
    }
    
    // 初始化 Re-ID
    reid_matcher = new ReIDMatcher(0.6f);  // 相似度閾值 0.6
    if (!reid_matcher->init(reid_model_data, reid_model_data_len)) {
        printf("Failed to initialize Re-ID matcher\n");
        return -1;
    }
    
    // 初始化 LCD 顯示
    lcd_display = new LCDDisplay();
    if (!lcd_display->init()) {
        printf("Warning: LCD display not available, continuing without visualization\n");
        delete lcd_display;
        lcd_display = nullptr;
    } else {
        printf("LCD display initialized.\n");
    }
    
    printf("\n========================================\n");
    printf(" System initialized, starting processing...\n");
    printf("========================================\n");
    
    // 分配幀緩衝區
    uint8_t* frame_buffer = new uint8_t[VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * 3];
    
    // 處理影片
    int frame_count = 0;
    while (video_controller->hasMoreFrames()) {
        if (video_controller->getNextFrame(frame_buffer)) {
            processFrame(frame_buffer, frame_count);
            frame_count++;
            
            // 可選:限制處理幀數
            // if (frame_count >= 100) break;
        }
    }
    
    // 輸出統計資訊
    printf("\n========================================\n");
    printf(" Processing Complete\n");
    printf("========================================\n");
    printf("Total frames processed: %d\n\n", frame_count);
    
    yolo_detector->printStats();
    printf("\n");
    reid_matcher->printStats();
    printf("\n");
    reid_matcher->printGallery();
    
    // 清理
    delete[] frame_buffer;
    delete video_controller;
    delete yolo_detector;
    delete reid_matcher;
    if (lcd_display) delete lcd_display;
    
    printf("\nDone!\n");
    
    return 0;
}