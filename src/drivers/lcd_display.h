/*
 * lcd_display.h - LCD 顯示驅動
 * 使用 VSI (Virtual Streaming Interface) 輸出到 FVP LCD
 */

#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// LCD 配置 - MPS3 板 LCD 解析度
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

class LCDDisplay {
public:
    LCDDisplay();
    ~LCDDisplay();
    
    // 初始化 LCD 顯示
    bool init();
    
    // 顯示一幀 (會自動縮放)
    void displayFrame(const uint8_t* frame, int width, int height);
    
    // 清除畫面
    void clear();
    
    // 獲取 LCD 尺寸
    int getWidth() const { return LCD_WIDTH; }
    int getHeight() const { return LCD_HEIGHT; }
    
private:
    uint8_t* lcd_buffer_;
    bool initialized_;
    
    // 縮放影像到 LCD 解析度
    void scaleToLCD(const uint8_t* src, int src_w, int src_h,
                    uint8_t* dst, int dst_w, int dst_h);
    
    // 寫入到 LCD (透過 MPS3 LCD controller 或 semihosting)
    void writeLCDBuffer();

    // MPS3 Shield LCD Helper Functions
    void wr_reg(uint8_t reg);
    void wr_dat(uint8_t dat);
};

#endif // LCD_DISPLAY_H
