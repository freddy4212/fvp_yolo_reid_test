/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <string.h>

#include "video_drv.h"
#include "arm_vsi.h"
#include "CMSIS_5/Device/ARM/ARMCM55/Include/ARMCM55.h"

// Video channel definitions
#define VIDEO_INPUT_CHANNELS    1
#define VIDEO_OUTPUT_CHANNELS   1

// Video peripheral definitions
// Using ARM_VSI0 for Video Input 0
#define VideoI0                 ARM_VSI0                   // Video Input channel 0 access struct
#define VideoI0_IRQn            ((IRQn_Type)224)           // Video Input channel 0 Interrupt number
#define VideoI0_Handler         Interrupt224_Handler       // Video Input channel 0 Interrupt handler

// Using ARM_VSI1 for Video Output 0
#define VideoO0                 ARM_VSI1                   // Video Output channel 0 access struct
#define VideoO0_IRQn            ((IRQn_Type)225)           // Video Output channel 0 Interrupt number
#define VideoO0_Handler         Interrupt225_Handler       // Video Output channel 0 Interrupt handler

// Video Peripheral registers
#define Reg_MODE                Regs[0]  // Mode: 0=Input, 1=Output
#define Reg_CONTROL             Regs[1]  // Control: enable, continuos, flush
#define Reg_STATUS              Regs[2]  // Status: active, buf_empty, buf_full, overflow, underflow, eos
#define Reg_FILENAME_LEN        Regs[3]  // Filename length
#define Reg_FILENAME_CHAR       Regs[4]  // Filename character
#define Reg_FILENAME_VALID      Regs[5]  // Filename valid flag
#define Reg_FRAME_WIDTH         Regs[6]  // Requested frame width
#define Reg_FRAME_HEIGHT        Regs[7]  // Requested frame height
#define Reg_COLOR_FORMAT        Regs[8]  // Color format
#define Reg_FRAME_RATE          Regs[9]  // Frame rate
#define Reg_FRAME_INDEX         Regs[10] // Frame index
#define Reg_FRAME_COUNT         Regs[11] // Frame count
#define Reg_FRAME_COUNT_MAX     Regs[12] // Frame count maximum

// Video Mode
#define Reg_MODE_Input          (0UL)
#define Reg_MODE_Output         (1UL)

// Video Control
#define Reg_CONTROL_ENABLE_Msk      (1UL << 0)
#define Reg_CONTROL_CONTINUOS_Msk   (1UL << 1)
#define Reg_CONTROL_FLUSH_Msk       (1UL << 2)

// Video Status
#define Reg_STATUS_ACTIVE_Msk       (1UL << 0)
#define Reg_STATUS_BUF_EMPTY_Msk    (1UL << 1)
#define Reg_STATUS_BUF_FULL_Msk     (1UL << 2)
#define Reg_STATUS_OVERFLOW_Msk     (1UL << 3)
#define Reg_STATUS_UNDERFLOW_Msk    (1UL << 4)
#define Reg_STATUS_EOS_Msk          (1UL << 5)

// Video IRQ Status
#define Reg_IRQ_Status_FRAME_Pos        0U
#define Reg_IRQ_Status_FRAME_Msk        (1UL << Reg_IRQ_Status_FRAME_Pos)
#define Reg_IRQ_Status_OVERFLOW_Pos     1U
#define Reg_IRQ_Status_OVERFLOW_Msk     (1UL << Reg_IRQ_Status_OVERFLOW_Pos)
#define Reg_IRQ_Status_UNDERFLOW_Pos    2U
#define Reg_IRQ_Status_UNDERFLOW_Msk    (1UL << Reg_IRQ_Status_UNDERFLOW_Pos)
#define Reg_IRQ_Status_EOS_Pos          3U
#define Reg_IRQ_Status_EOS_Msk          (1UL << Reg_IRQ_Status_EOS_Pos)

#define Reg_IRQ_Status_Msk              (Reg_IRQ_Status_FRAME_Msk     | \
                                         Reg_IRQ_Status_OVERFLOW_Msk  | \
                                         Reg_IRQ_Status_UNDERFLOW_Msk | \
                                         Reg_IRQ_Status_EOS_Msk)

// Video peripheral access structure
static ARM_VSI_Type * const pVideo[2] = { VideoI0, VideoO0 };

// Driver State
static uint8_t  Initialized = 0U;
static uint8_t  Configured[2] = { 0U, 0U };

// Event Callback
static VideoDrv_Event_t CB_Event = NULL;

// Video Interrupt Handler
static void Video_Handler (uint32_t channel) {
  ARM_VSI_Type *vsi = pVideo[channel];
  uint32_t status;
  uint32_t event;

  status = vsi->IRQ.Status;
  vsi->IRQ.Clear = status;

  if (CB_Event != NULL) {
    event = 0U;
    if ((status & Reg_IRQ_Status_FRAME_Msk) != 0U) {
      event |= VIDEO_DRV_EVENT_FRAME;
    }
    if ((status & Reg_IRQ_Status_OVERFLOW_Msk) != 0U) {
      event |= VIDEO_DRV_EVENT_OVERFLOW;
    }
    if ((status & Reg_IRQ_Status_UNDERFLOW_Msk) != 0U) {
      event |= VIDEO_DRV_EVENT_UNDERFLOW;
    }
    if ((status & Reg_IRQ_Status_EOS_Msk) != 0U) {
      event |= VIDEO_DRV_EVENT_EOS;
    }
    if (event != 0U) {
      CB_Event(channel, event);
    }
  }
}

// Video channel 0 Interrupt Handler
void VideoI0_Handler (void) {
  Video_Handler(0U);
}

// Video channel 1 Interrupt Handler
void VideoO0_Handler (void) {
  Video_Handler(1U);
}

// Initialize Video Interface
int32_t VideoDrv_Initialize (VideoDrv_Event_t cb_event) {

  if (Initialized) {
      return VIDEO_DRV_OK;
  }

  CB_Event = cb_event;

  // Initialize Video Input channel 0
  VideoI0->Timer.Control = 0U;
  VideoI0->DMA.Control   = 0U;
  VideoI0->IRQ.Clear     = Reg_IRQ_Status_Msk;
  VideoI0->IRQ.Enable    = Reg_IRQ_Status_Msk;
  VideoI0->Reg_MODE      = Reg_MODE_Input;
  VideoI0->Reg_CONTROL   = 0U;

  NVIC_ClearPendingIRQ(VideoI0_IRQn);
  NVIC_EnableIRQ(VideoI0_IRQn);

  // Initialize Video Output channel 0
  VideoO0->Timer.Control = 0U;
  VideoO0->DMA.Control   = 0U;
  VideoO0->IRQ.Clear     = Reg_IRQ_Status_Msk;
  VideoO0->IRQ.Enable    = Reg_IRQ_Status_Msk;
  VideoO0->Reg_MODE      = Reg_MODE_Output;
  VideoO0->Reg_CONTROL   = 0U;

  NVIC_ClearPendingIRQ(VideoO0_IRQn);
  NVIC_EnableIRQ(VideoO0_IRQn);

  Initialized = 1U;

  return VIDEO_DRV_OK;
}

// De-initialize Video Interface
int32_t VideoDrv_Uninitialize (void) {

  NVIC_DisableIRQ(VideoI0_IRQn);
  NVIC_DisableIRQ(VideoO0_IRQn);

  VideoI0->Timer.Control = 0U;
  VideoI0->DMA.Control   = 0U;
  VideoI0->IRQ.Enable    = 0U;
  VideoI0->IRQ.Clear     = Reg_IRQ_Status_Msk;
  VideoI0->Reg_CONTROL   = 0U;

  VideoO0->Timer.Control = 0U;
  VideoO0->DMA.Control   = 0U;
  VideoO0->IRQ.Enable    = 0U;
  VideoO0->IRQ.Clear     = Reg_IRQ_Status_Msk;
  VideoO0->Reg_CONTROL   = 0U;

  Initialized = 0U;

  return VIDEO_DRV_OK;
}

// Set Video channel file
int32_t VideoDrv_SetFile (uint32_t channel, const char *filename) {
  ARM_VSI_Type *vsi;
  uint32_t n;

  if (channel >= 2) {
    return VIDEO_DRV_ERROR_PARAMETER;
  }

  vsi = pVideo[channel];

  if (filename != NULL) {
    n = 0U;
    while (filename[n] != 0U) {
      n++;
    }
    vsi->Reg_FILENAME_LEN = n;

    n = 0U;
    while (filename[n] != 0U) {
      vsi->Reg_FILENAME_CHAR = filename[n++];
    }
  } else {
    vsi->Reg_FILENAME_LEN = 0U;
  }

  // Retry loop to allow Python side to update status
  int retries = 10000000;
  while (vsi->Reg_FILENAME_VALID == 0U && retries > 0) {
      retries--;
  }

  if (vsi->Reg_FILENAME_VALID == 0U) {
    return VIDEO_DRV_ERROR;
  }

  return VIDEO_DRV_OK;
}

// Configure Video channel
int32_t VideoDrv_Configure (uint32_t channel, uint32_t width, uint32_t height, uint32_t color_format, uint32_t frame_rate) {
  ARM_VSI_Type *vsi;

  if (channel >= 2) {
    return VIDEO_DRV_ERROR_PARAMETER;
  }

  vsi = pVideo[channel];

  vsi->Reg_FRAME_WIDTH  = width;
  vsi->Reg_FRAME_HEIGHT = height;
  vsi->Reg_COLOR_FORMAT = color_format;
  vsi->Reg_FRAME_RATE   = frame_rate;

  Configured[channel] = 1U;

  return VIDEO_DRV_OK;
}

// Set Video channel buffer
int32_t VideoDrv_SetBuf (uint32_t channel, void *buf, uint32_t buf_size) {
  ARM_VSI_Type *vsi;

  if (channel >= 2) {
    return VIDEO_DRV_ERROR_PARAMETER;
  }

  vsi = pVideo[channel];

  vsi->DMA.Address   = (uint32_t)buf;
  vsi->DMA.BlockSize = buf_size;
  vsi->DMA.BlockNum  = 1U;

  return VIDEO_DRV_OK;
}

// Start Video stream
int32_t VideoDrv_StreamStart (uint32_t channel, uint32_t mode) {
  ARM_VSI_Type *vsi;
  uint32_t control;

  if (channel >= 2) {
    return VIDEO_DRV_ERROR_PARAMETER;
  }

  if (Initialized == 0U) {
    return VIDEO_DRV_ERROR;
  }

  if (Configured[channel] == 0U) {
    return VIDEO_DRV_ERROR;
  }

  vsi = pVideo[channel];

  control = Reg_CONTROL_ENABLE_Msk;
  if (mode == VIDEO_DRV_MODE_CONTINUOS) {
    control |= Reg_CONTROL_CONTINUOS_Msk;
  }

  vsi->Reg_CONTROL = control;

  // Enable SysTick to trigger interrupts (30 FPS = 33ms)
  // Assuming 25MHz clock: 25,000,000 * 0.033 = 825,000
  SysTick_Config(825000);

  if ((vsi->Reg_STATUS & Reg_STATUS_ACTIVE_Msk) == 0U) {
    return VIDEO_DRV_ERROR;
  }

  return VIDEO_DRV_OK;
}

// Stop Video stream
int32_t VideoDrv_StreamStop (uint32_t channel) {
  ARM_VSI_Type *vsi;

  if (channel >= 2) {
    return VIDEO_DRV_ERROR_PARAMETER;
  }

  if (Initialized == 0U) {
    return VIDEO_DRV_ERROR;
  }

  vsi = pVideo[channel];

  vsi->Reg_CONTROL = 0U;
  
  // Disable SysTick
  SysTick->CTRL = 0;

  if ((vsi->Reg_STATUS & Reg_STATUS_ACTIVE_Msk) != 0U) {
    return VIDEO_DRV_ERROR;
  }

  return VIDEO_DRV_OK;
}

// SysTick Handler to simulate VSI Timer
void SysTick_Handler(void) {
    if (CB_Event != NULL) {
        CB_Event(0, VIDEO_DRV_EVENT_FRAME);
    }
}

// Get Video channel status
VideoDrv_Status_t VideoDrv_GetStatus (uint32_t channel) {
  ARM_VSI_Type *vsi;
  VideoDrv_Status_t status;
  uint32_t val;

  memset(&status, 0, sizeof(status));

  if (channel < 2) {
    vsi = pVideo[channel];
    val = vsi->Reg_STATUS;
    if ((val & Reg_STATUS_ACTIVE_Msk) != 0U) {
      status.active = 1U;
    }
    if ((val & Reg_STATUS_BUF_EMPTY_Msk) != 0U) {
      status.buf_empty = 1U;
    }
    if ((val & Reg_STATUS_BUF_FULL_Msk) != 0U) {
      status.buf_full = 1U;
    }
    if ((val & Reg_STATUS_OVERFLOW_Msk) != 0U) {
      status.overflow = 1U;
    }
    if ((val & Reg_STATUS_UNDERFLOW_Msk) != 0U) {
      status.underflow = 1U;
    }
    if ((val & Reg_STATUS_EOS_Msk) != 0U) {
      status.eos = 1U;
    }
  }

  return status;
}
