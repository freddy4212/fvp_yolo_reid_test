# Copyright (c) 2021-2024 Arm Limited. All rights reserved.

# Virtual Streaming Interface instance 1 Python script (Output)

import logging
import os
import sys

# Import the output version of vsi_video
import vsi_video_out as vsi_video

## Set verbosity level
#verbosity = logging.DEBUG
verbosity = logging.WARNING

# [debugging] Verbosity settings
level = { 10: "DEBUG",  20: "INFO",  30: "WARNING",  40: "ERROR" }
logging.basicConfig(format='Py: VSI1: [%(levelname)s]\t%(message)s', level = verbosity)
logging.info("Verbosity level is set to " + level[verbosity])


# Video Server configuration (Port 6001 for Output)
server_address = ('127.0.0.1', 6001)
server_authkey = 'vsi_video'


# IRQ registers
IRQ_Status = 0

# Timer registers
Timer_Control  = 0
Timer_Interval = 0

# Timer Control register definitions
Timer_Control_Run_Msk      = 1<<0
Timer_Control_Periodic_Msk = 1<<1
Timer_Control_Trig_IRQ_Msk = 1<<2
Timer_Control_Trig_DMA_Msk = 1<<3

# DMA registers
DMA_Control = 0
DMA_Address = 0
DMA_BlockSize = 0
DMA_BlockNum = 0

# DMA Control register definitions
DMA_Control_Enable_Msk    = 1<<0
DMA_Control_Direction_Msk = 1<<1
DMA_Control_Direction_P2M = 0<<1
DMA_Control_Direction_M2P = 1<<1

# User registers
Regs = [0] * 64

# Fake DMA Registers
REG_IDX_DMA_CONTROL = 20
REG_IDX_DMA_ADDR    = 21
REG_IDX_DMA_SIZE    = 22

# Data buffer
Data = bytearray()


## Initialize
#  @return None
def init():
    logging.info("Python function init() called")
    print("Py: VSI1: init() called")
    print("Py: VSI1: vsi_video file: {}".format(vsi_video.__file__))
    sys.stdout.flush()
    vsi_video.init(server_address, server_authkey)


## Read interrupt request (the VSI IRQ Status Register)
#  @return value value read (32-bit)
def rdIRQ():
    global IRQ_Status
    value = IRQ_Status
    return value


## Write interrupt request (the VSI IRQ Status Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrIRQ(value):
    global IRQ_Status
    logging.info("Python function wrIRQ() called")
    value = vsi_video.wrIRQ(IRQ_Status, value)
    IRQ_Status = value
    return value


## Read timer control (the VSI Timer Control Register)
#  @return value value read (32-bit)
def rdTimerControl():
    global Timer_Control
    value = Timer_Control
    return value


## Write timer control (the VSI Timer Control Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrTimerControl(value):
    global Timer_Control
    Timer_Control = value
    return value


## Read timer interval (the VSI Timer Interval Register)
#  @return value value read (32-bit)
def rdTimerInterval():
    global Timer_Interval
    value = Timer_Interval
    return value


## Write timer interval (the VSI Timer Interval Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrTimerInterval(value):
    global Timer_Interval
    Timer_Interval = value
    return value


## Timer Event
def timerEvent():
    global IRQ_Status
    IRQ_Status = vsi_video.timerEvent(IRQ_Status)
    return IRQ_Status


## Read DMA control (the VSI DMA Control Register)
#  @return value value read (32-bit)
def rdDMAControl():
    global DMA_Control
    value = DMA_Control
    return value


## Write DMA control (the VSI DMA Control Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrDMAControl(value):
    global DMA_Control
    logging.info("Python function wrDMAControl() called with value: {}".format(value))
    print("Py: VSI1: wrDMAControl called with value: {}".format(value))
    sys.stdout.flush()
    DMA_Control = value
    return value

def wrDMAAddress(value):
    global DMA_Address
    DMA_Address = value
    return value

def rdDMAAddress():
    global DMA_Address
    return DMA_Address

def wrDMABlockSize(value):
    global DMA_BlockSize
    DMA_BlockSize = value
    return value

def rdDMABlockSize():
    global DMA_BlockSize
    return DMA_BlockSize

def wrDMABlockNum(value):
    global DMA_BlockNum
    DMA_BlockNum = value
    return value

def rdDMABlockNum():
    global DMA_BlockNum
    return DMA_BlockNum


## Read data from peripheral for DMA P2M transfer (VSI DMA)
#  @param size size of data to read (in bytes, multiple of 4)
#  @return data data read (bytearray)
def rdDataDMA(size):
    global Data
    Data = vsi_video.rdDataDMA(size)
    n = min(len(Data), size)
    data = bytearray(size)
    data[0:n] = Data[0:n]
    return data


## Write data to peripheral for DMA M2P transfer (VSI DMA)
#  @param data data to write (bytearray)
#  @param size size of data to write (in bytes, multiple of 4)
def wrDataDMA(data, size):
    global Data
    logging.info("Python function wrDataDMA() called with size: {}".format(size))
    print("Py: VSI1: wrDataDMA called with size: {}".format(size))
    sys.stdout.flush()
    Data = data
    vsi_video.wrDataDMA(data, size)
    return


## Read user registers (the VSI User Registers)
#  @param index user register index (zero based)
#  @return value value read (32-bit)
def rdRegs(index):
    global Regs
    if index <= vsi_video.REG_IDX_MAX:
        Regs[index] = vsi_video.rdRegs(index)
    value = Regs[index]
    return value


## Write user registers (the VSI User Registers)
#  @param index user register index (zero based)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrRegs(index, value):
    global Regs
    try:
        logging.info("Python function wrRegs() called index: {} value: {}".format(index, value))

        # Fake DMA Trigger
        if index == REG_IDX_DMA_CONTROL:
             if (value & 4) == 4: # Trigger Bit (Bit 2)
                 logging.info("Py: VSI1: Fake DMA Triggered (Output)")
                 # Call vsi_video to process frame from file
                 vsi_video.process_frame_file_out()
                 
                 # Set Done Bit (Bit 3)
                 value |= 8
                 Regs[index] = value
                 return value

        if index <= vsi_video.REG_IDX_MAX:
            value = vsi_video.wrRegs(index, value)

        Regs[index] = value
        return value
    except Exception as e:
        logging.error("Exception in wrRegs: {}".format(e))
        import traceback
        traceback.print_exc()
        return 0

# Aliases for FVP compatibility
def wrTimer(value): return wrTimerControl(value)
def rdTimer(): return rdTimerControl()
def wrTimerIntervalAlias(value): return wrTimerInterval(value)
def rdTimerIntervalAlias(): return rdTimerInterval()
def wrDMA(value): return wrDMAControl(value)
def rdDMA(): return rdDMAControl()
def wrDMA_Control(value): return wrDMAControl(value)
def rdDMA_Control(): return rdDMAControl()
def wrTimer_Control(value): return wrTimerControl(value)
def rdTimer_Control(): return rdTimerControl()
