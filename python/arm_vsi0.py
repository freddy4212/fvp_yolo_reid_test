# Copyright (c) 2021-2024 Arm Limited. All rights reserved.

# Virtual Streaming Interface instance 0 Python script

import logging
import sys

import vsi_video

## Set verbosity level
#verbosity = logging.DEBUG
verbosity = logging.WARNING

# [debugging] Verbosity settings
level = { 10: "DEBUG",  20: "INFO",  30: "WARNING",  40: "ERROR" }
logging.basicConfig(format='Py: VSI0: [%(levelname)s]\t%(message)s', level = verbosity)
logging.info("Verbosity level is set to " + level[verbosity])


# Video Server configuration
server_address = ('127.0.0.1', 6000)
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
    print("Py: VSI0: init() called")
    print("Py: VSI0: vsi_video file: {}".format(vsi_video.__file__))
    print("Py: VSI0: vsi_video.REG_IDX_MAX: {}".format(vsi_video.REG_IDX_MAX))
    sys.stdout.flush()
    vsi_video.init(server_address, server_authkey)


## Read interrupt request (the VSI IRQ Status Register)
#  @return value value read (32-bit)
def rdIRQ():
    global IRQ_Status
    # logging.info("Python function rdIRQ() called")

    value = IRQ_Status
    # logging.debug("Read interrupt request: {}".format(value))

    return value


## Write interrupt request (the VSI IRQ Status Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrIRQ(value):
    global IRQ_Status
    logging.info("Python function wrIRQ() called")

    value = vsi_video.wrIRQ(IRQ_Status, value)
    IRQ_Status = value
    logging.debug("Write interrupt request: {}".format(value))

    return value


## Read timer control (the VSI Timer Control Register)
#  @return value value read (32-bit)
def rdTimerControl():
    global Timer_Control
    logging.info("Python function rdTimerControl() called")

    value = Timer_Control
    logging.debug("Read Timer_Control: {}".format(value))

    return value


## Write timer control (the VSI Timer Control Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrTimerControl(value):
    global Timer_Control
    logging.info("Python function wrTimerControl() called with value: {}".format(value))
    print("Py: VSI0: wrTimerControl called with value: {}".format(value))
    sys.stdout.flush()

    Timer_Control = value
    logging.debug("Write Timer_Control: {}".format(value))

    return value


## Read timer interval (the VSI Timer Interval Register)
#  @return value value read (32-bit)
def rdTimerInterval():
    global Timer_Interval
    logging.info("Python function rdTimerInterval() called")

    value = Timer_Interval
    logging.debug("Read Timer_Interval: {}".format(value))

    return value


## Write timer interval (the VSI Timer Interval Register)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrTimerInterval(value):
    global Timer_Interval
    logging.info("Python function wrTimerInterval() called with value: {}".format(value))
    print("Py: VSI0: wrTimerInterval called with value: {}".format(value))
    sys.stdout.flush()

    Timer_Interval = value
    logging.debug("Write Timer_Interval: {}".format(value))

    return value


## Timer Event
def timerEvent():
    global IRQ_Status

    logging.info("Python function timerEvent() called")
    print("Py: VSI0: timerEvent called")
    sys.stdout.flush()

    IRQ_Status = vsi_video.timerEvent(IRQ_Status)
    return IRQ_Status


## Read data from peripheral for DMA P2M transfer (VSI DMA)
#  @param size size of data to read (in bytes, multiple of 4)
#  @return data data read (bytearray)
def rdDataDMA(size):
    global Data
    logging.info("Python function rdDataDMA() called")
    print("Py: VSI0: rdDataDMA called")
    sys.stdout.flush()

    Data = vsi_video.rdDataDMA(size)

    n = min(len(Data), size)
    data = bytearray(size)
    data[0:n] = Data[0:n]
    logging.debug("Read data ({} bytes)".format(size))

    return data


## Write data to peripheral for DMA M2P transfer (VSI DMA)
#  @param data data to write (bytearray)
#  @param size size of data to write (in bytes, multiple of 4)
def wrDataDMA(data, size):
    global Data
    logging.info("Python function wrDataDMA() called")

    Data = data
    logging.debug("Write data ({} bytes)".format(size))

    vsi_video.wrDataDMA(data, size)

    return



## Read user registers (the VSI User Registers)
#  @param index user register index (zero based)
#  @return value value read (32-bit)
def rdRegs(index):
    global Regs
    # logging.info("Python function rdRegs() called")

    if index <= vsi_video.REG_IDX_MAX:
        Regs[index] = vsi_video.rdRegs(index)

    value = Regs[index]
    # logging.debug("Read user register at index {}: {}".format(index, value))

    return value


## Write user registers (the VSI User Registers)
#  @param index user register index (zero based)
#  @param value value to write (32-bit)
#  @return value value written (32-bit)
def wrRegs(index, value):
    global Regs
    try:
        logging.info("Python function wrRegs() called index: {} value: {}".format(index, value))
        # logging.info("Type of index: {}, Type of MAX: {}, MAX: {}".format(type(index), type(vsi_video.REG_IDX_MAX), vsi_video.REG_IDX_MAX))

        # Fake DMA Trigger
        if index == REG_IDX_DMA_CONTROL:
             if (value & 1) == 1: # Trigger Bit
                 logging.info("Py: VSI0: Fake DMA Triggered (Input)")
                 # Call vsi_video to process frame and write to file
                 vsi_video.process_frame_file()
                 
                 # Set Done Bit (Bit 1)
                 value |= 2
                 Regs[index] = value
                 return value

        if index <= vsi_video.REG_IDX_MAX:
            logging.info("Py: VSI0: Calling vsi_video.wrRegs({}, {})".format(index, value))
            value = vsi_video.wrRegs(index, value)
            # if index == 4:
            #      logging.info("Py: VSI0: Idx: {} Len: {}".format(vsi_video.FilenameIdx, vsi_video.FILENAME_LEN))

        Regs[index] = value
        logging.debug("Write user register at index {}: {}".format(index, value))

        return value
    except Exception as e:
        logging.error("Exception in wrRegs: {}".format(e))
        import traceback
        traceback.print_exc()
        return 0

# Aliases for FVP compatibility
def wrTimer(value):
    logging.info("wrTimer called (alias)")
    print("Py: VSI0: wrTimer called (alias)")
    sys.stdout.flush()
    return wrTimerControl(value)

def rdTimer():
    return rdTimerControl()

def wrTimerIntervalAlias(value):
    logging.info("wrTimerInterval called (alias)")
    print("Py: VSI0: wrTimerInterval called (alias)")
    sys.stdout.flush()
    return wrTimerInterval(value)

def rdTimerIntervalAlias():
    return rdTimerInterval()

def wrDMA(value):
    logging.info("wrDMA called (alias)")
    print("Py: VSI0: wrDMA called (alias)")
    sys.stdout.flush()
    return wrDMAControl(value)

def rdDMA():
    return rdDMAControl()

# More aliases just in case
def wrDMA_Control(value):
    return wrDMAControl(value)

def rdDMA_Control():
    return rdDMAControl()

def wrTimer_Control(value):
    return wrTimerControl(value)

def rdTimer_Control():
    return rdTimerControl()

# More aliases just in case
def wrDMA_Control(value):
    return wrDMAControl(value)

def rdDMA_Control():
    return rdDMAControl()

def wrTimer_Control(value):
    return wrTimerControl(value)

def rdTimer_Control():
    return rdTimerControl()

# More aliases just in case
def wrDMA_Control(value):
    return wrDMAControl(value)

def rdDMA_Control():
    return rdDMAControl()

def wrTimer_Control(value):
    return wrTimerControl(value)

def rdTimer_Control():
    return rdTimerControl()

# More aliases just in case
def wrDMA_Control(value):
    return wrDMAControl(value)

def rdDMA_Control():
    return rdDMAControl()

def wrTimer_Control(value):
    return wrTimerControl(value)

def rdTimer_Control():
    return rdTimerControl()
