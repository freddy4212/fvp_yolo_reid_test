#include "vsi_video.h"
#include "video_drv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define Fake DMA Register Indices
#define REG_IDX_DMA_CONTROL 20
#define REG_IDX_DMA_ADDRESS 21
#define REG_IDX_DMA_SIZE    22

extern "C" {
#include "arm_vsi.h"
}

// VSI Video Channel Definition
#define VSI_VIDEO_CHANNEL VIDEO_DRV_IN0

static uint8_t static_frame_buffer[VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS] __attribute__((section(".ddr_data"), aligned(16)));

// Volatile flag for frame ready
static volatile uint32_t frame_ready = 0;
static FILE* frame_file = nullptr;

// Callback function for Video Driver
static void VideoDrv_Callback(uint32_t channel, uint32_t event) {
    if (channel == VSI_VIDEO_CHANNEL) {
        if (event & VIDEO_DRV_EVENT_FRAME) {
            frame_ready = 1;
        }
    }
}

VSIVideoController::VSIVideoController(const char* video_path)
    : video_path_(video_path)
    , frame_count_(0)
    , total_frames_(0)
    , frame_buffer_(nullptr)
    , initialized_(false)
    , vsi_handle_(nullptr)
{
    frame_buffer_ = static_frame_buffer;
}

VSIVideoController::~VSIVideoController() {
    if (frame_file) {
        fclose(frame_file);
        frame_file = nullptr;
    }
    VideoDrv_StreamStop(VSI_VIDEO_CHANNEL);
    VideoDrv_Uninitialize();
}

bool VSIVideoController::init() {
    printf("[VSI] Initializing video source: %s\n", video_path_);
    
    // Initialize Video Driver
    if (VideoDrv_Initialize(VideoDrv_Callback) != VIDEO_DRV_OK) {
        printf("[VSI] Failed to initialize Video Driver\n");
        return false;
    }

    // Set Video File
    if (VideoDrv_SetFile(VSI_VIDEO_CHANNEL, video_path_) != VIDEO_DRV_OK) {
        printf("[VSI] Failed to set video file\n");
        return false;
    }
    
    // Configure Video
    if (VideoDrv_Configure(VSI_VIDEO_CHANNEL, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT, VIDEO_DRV_COLOR_RGB888, 30) != VIDEO_DRV_OK) {
        printf("[VSI] Failed to configure video\n");
        return false;
    }

    // Set Buffer
    if (VideoDrv_SetBuf(VSI_VIDEO_CHANNEL, frame_buffer_, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS) != VIDEO_DRV_OK) {
        printf("[VSI] Failed to set video buffer\n");
        return false;
    }
    
    // Open frame file for Fake DMA
    frame_file = fopen("frame_buffer.bin", "rb");
    if (!frame_file) {
        // Try to create it if it doesn't exist
        frame_file = fopen("frame_buffer.bin", "wb+");
        if (frame_file) {
             // Pre-allocate?
        }
    }
    
    total_frames_ = 100; // Placeholder
    
    printf("[VSI] Video initialized: %dx%d\n", VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT);
    
    initialized_ = true;
    frame_count_ = 0;

    // Start Stream (Continuous) - Keep stream open for performance
    if (VideoDrv_StreamStart(VSI_VIDEO_CHANNEL, VIDEO_DRV_MODE_CONTINUOS) != VIDEO_DRV_OK) {
        printf("[VSI] Failed to start video stream\n");
        return false;
    }
    
    return true;
}

bool VSIVideoController::getNextFrame(uint8_t* frame_buffer) {
    if (!initialized_) {
        printf("[VSI] Video not initialized\n");
        return false;
    }
    
    // Trigger Fake DMA (File I/O)
    // Set Size
    ARM_VSI0->Regs[REG_IDX_DMA_SIZE] = VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS;
    
    // Trigger (Enable = 1)
    ARM_VSI0->Regs[REG_IDX_DMA_CONTROL] = 1;
    
    // Wait for Ready (Bit 1 = 2)
    int timeout = 10000000;
    while ((ARM_VSI0->Regs[REG_IDX_DMA_CONTROL] & 2) == 0) {
        timeout--;
        if (timeout == 0) {
            printf("[VSI] Timeout waiting for Fake DMA\n");
            break;
        }
    }
    
    if (timeout > 0) {
        if (frame_file) {
            // Use persistent file handle
            // Rewind to beginning
            fseek(frame_file, 0, SEEK_SET);
            fread(frame_buffer, 1, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS, frame_file);
        } else {
             frame_file = fopen("frame_buffer.bin", "rb");
             if (frame_file) {
                fread(frame_buffer, 1, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS, frame_file);
             }
        }
    }

    // Reset Control
    ARM_VSI0->Regs[REG_IDX_DMA_CONTROL] = 0;

    frame_count_++;
    
    if (frame_count_ % 30 == 0) {
        printf("[VSI] Processed frame %d\n", frame_count_);
    }
    
    return true;
}

void VSIVideoController::reset() {
    frame_count_ = 0;
    // VideoDrv doesn't have a reset function exposed, but StreamStop might help?
    // Or we just rely on the Python side to handle it.
}

bool VSIVideoController::hasMoreFrames() const {
    // Since we don't know the total frames for sure, we can just return true
    // or check if we hit an EOS event (which we should handle in the callback).
    // For now, just return true.
    return true; 
}

// ==========================================
// VSIVideoOutput Implementation
// ==========================================

#define VSI_VIDEO_CHANNEL_OUT VIDEO_DRV_OUT0

static uint8_t static_output_buffer[VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS] __attribute__((section(".ddr_data"), aligned(16)));

VSIVideoOutput::VSIVideoOutput() : initialized_(false), output_buffer_(static_output_buffer) {}

VSIVideoOutput::~VSIVideoOutput() {
    VideoDrv_StreamStop(VSI_VIDEO_CHANNEL_OUT);
}

bool VSIVideoOutput::init() {
    printf("[VSI Out] Initializing video output\n");

    // Ensure Video Driver is initialized (idempotent check inside VideoDrv_Initialize)
    if (VideoDrv_Initialize(VideoDrv_Callback) != VIDEO_DRV_OK) {
        printf("[VSI Out] Failed to initialize Video Driver\n");
        return false;
    }

    // Configure Video Output
    if (VideoDrv_Configure(VSI_VIDEO_CHANNEL_OUT, VSI_VIDEO_WIDTH, VSI_VIDEO_HEIGHT, VIDEO_DRV_COLOR_RGB888, 30) != VIDEO_DRV_OK) {
        printf("[VSI Out] Failed to configure video output\n");
        return false;
    }

    // Set Buffer
    if (VideoDrv_SetBuf(VSI_VIDEO_CHANNEL_OUT, output_buffer_, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS) != VIDEO_DRV_OK) {
        printf("[VSI Out] Failed to set video output buffer\n");
        return false;
    }

    // Start Stream
    if (VideoDrv_StreamStart(VSI_VIDEO_CHANNEL_OUT, VIDEO_DRV_MODE_CONTINUOS) != VIDEO_DRV_OK) {
        printf("[VSI Out] Failed to start video output stream\n");
        return false;
    }

    initialized_ = true;
    return true;
}

bool VSIVideoOutput::sendFrame(const uint8_t* frame_buffer) {
    if (!initialized_) return false;

    // Use Semihosting to write frame to file (Fake DMA)
    FILE* f = fopen("frame_buffer_out.bin", "wb");
    if (f) {
        fwrite(frame_buffer, 1, VSI_VIDEO_WIDTH * VSI_VIDEO_HEIGHT * VSI_VIDEO_CHANNELS, f);
        fclose(f);
    } else {
        printf("[VSI Out] Failed to open frame_buffer_out.bin\n");
        return false;
    }

    // Trigger Fake DMA via User Registers
    // Trigger Bit = 4 (Bit 2)
    ARM_VSI1->Regs[REG_IDX_DMA_CONTROL] = 4;
    
    // Wait for Done Bit = 8 (Bit 3)
    int timeout = 10000000;
    while ((ARM_VSI1->Regs[REG_IDX_DMA_CONTROL] & 8) == 0) {
        timeout--;
        if (timeout == 0) {
            printf("[VSI Out] Timeout waiting for Fake DMA\n");
            break;
        }
    }
    
    // Reset Control
    ARM_VSI1->Regs[REG_IDX_DMA_CONTROL] = 0;
    
    return true;
}
