/*
 * lcd_display.cpp - MPS3 Shield LCD Driver Implementation
 * 
 * Based on glcd_mps3.c from ARM AVH-VSI
 * Controls the MPS3 Shield LCD via SPI-like interface at 0x5930A000
 */

#include "lcd_display.h"
#include <cstring>
#include <cstdio>
#include "CMSIS_5/Device/ARM/ARMCM55/Include/ARMCM55.h"

// MPS3 Shield LCD Registers (Non-Secure Alias)
// Corstone-300 (SSE-300) Expansion 1 (Shield) is at 0x49300000 (NS) / 0x59300000 (S)
// The SCC (Serial Communication Controller) for the Shield is typically at offset 0xA000
#define MPS3_SCC_BASE   0x4930A000
#define CHAR_COM        (*(volatile uint32_t *)(MPS3_SCC_BASE + 0x000))
#define CHAR_DAT        (*(volatile uint32_t *)(MPS3_SCC_BASE + 0x004))
#define CHAR_RD         (*(volatile uint32_t *)(MPS3_SCC_BASE + 0x008))
#define CHAR_MISC       (*(volatile uint32_t *)(MPS3_SCC_BASE + 0x04C))

// Control Bits
#define CHAR_MISC_RST   (1UL << 0)
#define CHAR_MISC_CS    (1UL << 1)
#define CHAR_MISC_BL    (1UL << 2)
#define CHAR_MISC_RD    (1UL << 3)

LCDDisplay::LCDDisplay() 
    : lcd_buffer_(nullptr), initialized_(false) {
}

LCDDisplay::~LCDDisplay() {
    if (lcd_buffer_) {
        delete[] lcd_buffer_;
    }
}

// Write Command to LCD Controller
void LCDDisplay::wr_reg(uint8_t reg) {
    // CS=0 (Active), RST=1, BL=1
    CHAR_MISC = CHAR_MISC_RST | CHAR_MISC_BL; 
    CHAR_COM  = reg;
    // CS=1 (Inactive), RST=1, BL=1
    CHAR_MISC = CHAR_MISC_RST | CHAR_MISC_BL | CHAR_MISC_CS; 
}

// Write Data to LCD Controller
void LCDDisplay::wr_dat(uint8_t dat) {
    // CS=0 (Active), RST=1, BL=1
    CHAR_MISC = CHAR_MISC_RST | CHAR_MISC_BL; 
    CHAR_DAT  = dat;
    // CS=1 (Inactive), RST=1, BL=1
    CHAR_MISC = CHAR_MISC_RST | CHAR_MISC_BL | CHAR_MISC_CS; 
}

bool LCDDisplay::init() {
    // Allocate buffer for software scaling (RGB888)
    lcd_buffer_ = new uint8_t[LCD_WIDTH * LCD_HEIGHT * 3];
    if (!lcd_buffer_) {
        printf("[LCD] Failed to allocate LCD buffer\n");
        return false;
    }
    memset(lcd_buffer_, 0, LCD_WIDTH * LCD_HEIGHT * 3);

    printf("[LCD] Initializing MPS3 Shield LCD at 0x%08X...\n", MPS3_SCC_BASE);

    // Reset Sequence
    CHAR_MISC = 0;              // RST=0, CS=0
    for(volatile int i=0; i<10000; i++);
    CHAR_MISC = CHAR_MISC_RST;  // RST=1
    for(volatile int i=0; i<10000; i++);
    // End of Reset: RST=1, BL=1, CS=1 (Inactive)
    CHAR_MISC = CHAR_MISC_RST | CHAR_MISC_BL | CHAR_MISC_CS; 

    // Initialization Sequence (from glcd_mps3.c)
    wr_reg(0x01); // Software Reset
    for(volatile int i=0; i<100000; i++);

    wr_reg(0x28); // Display OFF

    // Power Control A
    wr_reg(0xCB); wr_dat(0x39); wr_dat(0x2C); wr_dat(0x00); wr_dat(0x34); wr_dat(0x02);
    // Power Control B
    wr_reg(0xCF); wr_dat(0x00); wr_dat(0xC1); wr_dat(0x30);
    // Driver Timing Control A
    wr_reg(0xE8); wr_dat(0x85); wr_dat(0x00); wr_dat(0x78);
    // Driver Timing Control B
    wr_reg(0xEA); wr_dat(0x00); wr_dat(0x00);
    // Power on Sequence Control
    wr_reg(0xED); wr_dat(0x64); wr_dat(0x03); wr_dat(0x12); wr_dat(0x81);
    // Pump Ratio Control
    wr_reg(0xF7); wr_dat(0x20);
    // Power Control 1
    wr_reg(0xC0); wr_dat(0x23);
    // Power Control 2
    wr_reg(0xC1); wr_dat(0x10);
    // VCOM Control 1
    wr_reg(0xC5); wr_dat(0x3E); wr_dat(0x28);
    // VCOM Control 2
    wr_reg(0xC7); wr_dat(0x86);

    // Memory Access Control (Orientation)
    // 0x48 = Row/Col Exchange (Landscape) | BGR
    // Adjust based on actual orientation needed. 0x28 or 0x48 is common for landscape.
    wr_reg(0x36); wr_dat(0x48); 

    // Pixel Format Set (16-bit RGB565)
    wr_reg(0x3A); wr_dat(0x55);

    // Frame Rate Control
    wr_reg(0xB1); wr_dat(0x00); wr_dat(0x18);
    // Display Function Control
    wr_reg(0xB6); wr_dat(0x08); wr_dat(0x82); wr_dat(0x27);
    // 3Gamma Function Disable
    wr_reg(0xF2); wr_dat(0x00);
    // Gamma Curve Selected
    wr_reg(0x26); wr_dat(0x01);
    // Positive Gamma Correction
    wr_reg(0xE0); 
    wr_dat(0x0F); wr_dat(0x31); wr_dat(0x2B); wr_dat(0x0C); wr_dat(0x0E); wr_dat(0x08); 
    wr_dat(0x4E); wr_dat(0xF1); wr_dat(0x37); wr_dat(0x07); wr_dat(0x10); wr_dat(0x03); 
    wr_dat(0x0E); wr_dat(0x09); wr_dat(0x00);
    // Negative Gamma Correction
    wr_reg(0xE1); 
    wr_dat(0x00); wr_dat(0x0E); wr_dat(0x14); wr_dat(0x03); wr_dat(0x11); wr_dat(0x07); 
    wr_dat(0x31); wr_dat(0xC1); wr_dat(0x48); wr_dat(0x08); wr_dat(0x0F); wr_dat(0x0C); 
    wr_dat(0x31); wr_dat(0x36); wr_dat(0x0F);

    wr_reg(0x11); // Sleep Out
    for(volatile int i=0; i<100000; i++);

    wr_reg(0x29); // Display ON
    
    printf("[LCD] MPS3 Shield LCD Initialized\n");
    initialized_ = true;
    return true;
}

void LCDDisplay::scaleToLCD(const uint8_t* src, int src_w, int src_h,
                            uint8_t* dst, int dst_w, int dst_h) {
    // Simple bilinear interpolation
    float x_ratio = (float)(src_w - 1) / dst_w;
    float y_ratio = (float)(src_h - 1) / dst_h;
    
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);
            
            float x_diff = (x * x_ratio) - src_x;
            float y_diff = (y * y_ratio) - src_y;
            
            int idx = (src_y * src_w + src_x) * 3;
            int idx_right = idx + 3;
            int idx_down = idx + src_w * 3;
            int idx_diag = idx_down + 3;
            
            for (int c = 0; c < 3; c++) {
                float val = 
                    src[idx + c] * (1 - x_diff) * (1 - y_diff) +
                    src[idx_right + c] * x_diff * (1 - y_diff) +
                    src[idx_down + c] * (1 - x_diff) * y_diff +
                    src[idx_diag + c] * x_diff * y_diff;
                
                dst[(y * dst_w + x) * 3 + c] = (uint8_t)val;
            }
        }
    }
}

void LCDDisplay::displayFrame(const uint8_t* frame, int width, int height) {
    if (!initialized_) return;
    
    // Scale to LCD resolution (320x240)
    scaleToLCD(frame, width, height, lcd_buffer_, LCD_WIDTH, LCD_HEIGHT);
    
    // Set Column Address (0 to 319)
    wr_reg(0x2A);
    wr_dat(0 >> 8); wr_dat(0 & 0xFF);
    wr_dat((LCD_WIDTH - 1) >> 8); wr_dat((LCD_WIDTH - 1) & 0xFF);
    
    // Set Page Address (0 to 239)
    wr_reg(0x2B);
    wr_dat(0 >> 8); wr_dat(0 & 0xFF);
    wr_dat((LCD_HEIGHT - 1) >> 8); wr_dat((LCD_HEIGHT - 1) & 0xFF);
    
    // Memory Write
    wr_reg(0x2C);
    
    // Write Pixel Data (RGB888 -> RGB565)
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        uint8_t r = lcd_buffer_[i * 3 + 0];
        uint8_t g = lcd_buffer_[i * 3 + 1];
        uint8_t b = lcd_buffer_[i * 3 + 2];
        
        // Convert to RGB565
        uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        
        wr_dat(color >> 8);
        wr_dat(color & 0xFF);
    }
}

void LCDDisplay::writeLCDBuffer() {
    // Not used in this driver implementation as we write directly to registers
}

void LCDDisplay::clear() {
    if (!initialized_) return;
    
    // Set Window
    wr_reg(0x2A);
    wr_dat(0); wr_dat(0);
    wr_dat((LCD_WIDTH - 1) >> 8); wr_dat((LCD_WIDTH - 1) & 0xFF);
    
    wr_reg(0x2B);
    wr_dat(0); wr_dat(0);
    wr_dat((LCD_HEIGHT - 1) >> 8); wr_dat((LCD_HEIGHT - 1) & 0xFF);
    
    wr_reg(0x2C);
    
    // Write Black
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        wr_dat(0);
        wr_dat(0);
    }
}
