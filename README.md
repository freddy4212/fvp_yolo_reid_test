# FVP YOLO ReID Test

This project demonstrates YOLOv8 Pose estimation and Person Re-Identification running on ARM Corstone-300 FVP with Ethos-U55 NPU.

## Project Structure

- `src/`: C++ source code for the application.
  - `ai/`: AI model logic (YOLO, ReID).
  - `drivers/`: Hardware drivers (VSI, LCD).
  - `utils/`: Utility functions (Image, Draw).
  - `platform/`: Platform specific code.
- `python/`: Python scripts for VSI (Virtual Streaming Interface) video server.
- `scripts/`: Helper scripts for installing dependencies.
- `models/`: TFLite model files.
- `test_videos/`: Test video files.
- `CMakeLists.txt`: CMake build configuration.
- `run_fvp.sh`: Main script to build and run the simulation.

## Prerequisites

- ARM GNU Toolchain (arm-none-eabi-gcc)
- ARM FVP (Fixed Virtual Platform) Corstone-300
- CMake
- Python 3

## Setup

1. Install dependencies:
   ```bash
   ./scripts/install_tflm_dep.sh
   ./scripts/install_tflm.sh
   ```

## Build and Run

Run the main script to build and execute on FVP:

```bash
./run_fvp.sh
```

This script will:
1. Configure the project using CMake.
2. Build the application.
3. Start the VSI video server.
4. Run the FVP simulation.

## Configuration

You can modify `run_fvp.sh` to change:
- `YOLO_MODEL`: YOLO model file.
- `REID_MODEL`: ReID model file.
- `VIDEO_FILE`: Input video file.
- `GUI_MODE`: Enable/disable LCD visualization.
