#include "yolo_pose.h"
#include "image_utils.h"
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <algorithm>

extern "C" uint32_t SystemCoreClock;

// TensorFlow Lite Micro
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace tflite {
extern TFLMRegistration* Register_ETHOSU();
extern const char* GetString_ETHOSU();
}

#define YOLO_TENSOR_ARENA_SIZE (1024 * 1024)  // 1MB
static uint8_t yolo_tensor_arena[YOLO_TENSOR_ARENA_SIZE] __attribute__((section(".ddr_data"), aligned(16)));

// YOLOv8 後處理參數
#define MODEL_SCORE_THRESHOLD 0.25f
#define MODEL_NMS_THRESHOLD 0.6f
#define NUM_OUTPUTS 7

YoloPoseDetector::YoloPoseDetector()
    : interpreter_(nullptr)
    , input_tensor_(nullptr)
    , tensor_arena_(nullptr)
    , total_inferences_(0)
    , total_inference_time_(0.0f)
    , stride_array_(nullptr)
    , anchor_array_(nullptr)
    , total_anchors_(0)
{
    tensor_arena_ = yolo_tensor_arena;
    out_dim_size_[0] = 0;
    out_dim_size_[1] = 0;
    out_dim_size_[2] = 0;
}

YoloPoseDetector::~YoloPoseDetector() {
    freeAnchorsAndStrides();
}

void YoloPoseDetector::initAnchorsAndStrides() {
    // 計算總 anchor 數量（256x256 輸入）
    // stride 8: 32x32 = 1024
    // stride 16: 16x16 = 256
    // stride 32: 8x8 = 64
    // 總計: 1344
    int dim_stride_8_size = (YOLO_INPUT_WIDTH / 8) * (YOLO_INPUT_HEIGHT / 8);   // 1024
    int dim_stride_16_size = (YOLO_INPUT_WIDTH / 16) * (YOLO_INPUT_HEIGHT / 16); // 256
    int dim_stride_32_size = (YOLO_INPUT_WIDTH / 32) * (YOLO_INPUT_HEIGHT / 32); // 64
    int dim_total_size = dim_stride_8_size + dim_stride_16_size + dim_stride_32_size; // 1344
    
    total_anchors_ = dim_total_size;
    
    // 分配 stride 陣列
    stride_array_ = (float*)calloc(dim_total_size, sizeof(float));
    
    // 分配 anchor 陣列
    anchor_array_ = (float**)calloc(dim_total_size, sizeof(float*));
    for (int i = 0; i < dim_total_size; i++) {
        anchor_array_[i] = (float*)calloc(2, sizeof(float));
    }
    
    // 記錄尺度邊界
    out_dim_size_[0] = dim_stride_8_size;                          // 1024
    out_dim_size_[1] = dim_stride_8_size + dim_stride_16_size;     // 1280
    out_dim_size_[2] = dim_total_size;                             // 1344
    
    // 填充 stride 陣列和 anchor 陣列
    int strides[] = {8, 16, 32};
    int sizes[] = {dim_stride_8_size, dim_stride_16_size, dim_stride_32_size};
    int start_step = 0;
    
    for (int scale = 0; scale < 3; scale++) {
        int stride = strides[scale];
        int grid_size = YOLO_INPUT_WIDTH / stride;
        int size = sizes[scale];
        
        for (int i = 0; i < size; i++) {
            int idx = start_step + i;
            stride_array_[idx] = (float)stride;
            
            // anchor 座標
            int y = i / grid_size;
            int x = i % grid_size;
            anchor_array_[idx][0] = x + 0.5f;
            anchor_array_[idx][1] = y + 0.5f;
        }
        start_step += size;
    }
    
    printf("[YOLO] Anchors and strides initialized (%d total)\n", dim_total_size);
    printf("[YOLO] Scale boundaries: %d, %d, %d\n", out_dim_size_[0], out_dim_size_[1], out_dim_size_[2]);
}

void YoloPoseDetector::freeAnchorsAndStrides() {
    if (stride_array_) {
        free(stride_array_);
        stride_array_ = nullptr;
    }
    
    if (anchor_array_) {
        for (int i = 0; i < total_anchors_; i++) {
            if (anchor_array_[i]) {
                free(anchor_array_[i]);
            }
        }
        free(anchor_array_);
        anchor_array_ = nullptr;
    }
}

float YoloPoseDetector::sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

void YoloPoseDetector::softmax(float* input, size_t len) {
    float m = -INFINITY;
    for (size_t i = 0; i < len; i++) {
        if (input[i] > m) m = input[i];
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < len; i++) {
        sum += expf(input[i] - m);
    }
    
    float offset = m + logf(sum);
    for (size_t i = 0; i < len; i++) {
        input[i] = expf(input[i] - offset);
    }
}

float YoloPoseDetector::dequantize(int8_t value, float scale, int32_t zero_point) {
    return ((float)value - (float)zero_point) * scale;
}

float YoloPoseDetector::getBboxDequantValue(int dim1, int dim2, void* tensor) {
    auto* t = (TfLiteTensor*)tensor;
    int8_t value = t->data.int8[dim2 + dim1 * t->dims->data[2]];
    auto* quant = (TfLiteAffineQuantization*)(t->quantization.params);
    float scale = quant->scale->data[0];
    int32_t zero_point = quant->zero_point->data[0];
    return dequantize(value, scale, zero_point);
}

float YoloPoseDetector::getKeypointDequantValue(int dim1, int dim2, void* tensor,
                                                  float anchor0, float anchor1, float stride) {
    auto* t = (TfLiteTensor*)tensor;
    int8_t value = t->data.int8[dim2 + dim1 * t->dims->data[2]];
    auto* quant = (TfLiteAffineQuantization*)(t->quantization.params);
    float scale = quant->scale->data[0];
    int32_t zero_point = quant->zero_point->data[0];
    float deq_value = dequantize(value, scale, zero_point);
    
    // 根據 keypoint 類型處理
    if (dim2 % 3 == 0) {
        // x 座標
        deq_value = (deq_value * 2.0f + (anchor0 - 0.5f)) * stride;
    } else if (dim2 % 3 == 1) {
        // y 座標
        deq_value = (deq_value * 2.0f + (anchor1 - 0.5f)) * stride;
    } else {
        // 置信度
        deq_value = sigmoid(deq_value);
    }
    
    return deq_value;
}

void YoloPoseDetector::calculateXYWH(int j, void** outputs, Box* bbox, int* out_dim_size) {
    float xywh_result[4];
    
    // DFL (Distribution Focal Loss) 解碼
    // 根據實際輸出張量維度:
    // Output[1] (1024 x 64) = stride 8 的 bbox
    // Output[0] (256 x 64) = stride 16 的 bbox
    // Output[5] (64 x 64) = stride 32 的 bbox
    
    for (int k = 0; k < 4; k++) {
        float tmp_arr[16];
        float result = 0.0f;
        
        for (int i = 0; i < 16; i++) {
            int output_idx;
            int local_j;
            float tmp_result;
            
            if (j < out_dim_size[0]) {
                // stride 8
                output_idx = 1;
                local_j = j;
            } else if (j < out_dim_size[1]) {
                // stride 16
                output_idx = 0;
                local_j = j - out_dim_size[0];
            } else {
                // stride 32
                output_idx = 5;
                local_j = j - out_dim_size[1];
            }
            tmp_result = getBboxDequantValue(local_j, k * 16 + i, outputs[output_idx]);
            tmp_arr[i] = tmp_result;
        }
        
        // Softmax
        softmax(tmp_arr, 16);
        
        // 加權平均
        for (int i = 0; i < 16; i++) {
            result += tmp_arr[i] * (float)i;
        }
        xywh_result[k] = result;
    }
    
    // dist2bbox * stride
    float x1 = anchor_array_[j][0] - xywh_result[0];
    float y1 = anchor_array_[j][1] - xywh_result[1];
    float x2 = anchor_array_[j][0] + xywh_result[2];
    float y2 = anchor_array_[j][1] + xywh_result[3];
    
    float cx = (x1 + x2) / 2.0f;
    float cy = (y1 + y2) / 2.0f;
    float w = x2 - x1;
    float h = y2 - y1;
    
    // 乘以 stride
    cx *= stride_array_[j];
    cy *= stride_array_[j];
    w *= stride_array_[j];
    h *= stride_array_[j];
    
    // 轉換為左上角座標
    bbox->x = cx - 0.5f * w;
    bbox->y = cy - 0.5f * h;
    bbox->w = w;
    bbox->h = h;
}

float YoloPoseDetector::boxIou(const Box& a, const Box& b) {
    // 計算交集
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.w, b.x + b.w);
    float y2 = std::min(a.y + a.h, b.y + b.h);
    
    float inter_w = std::max(0.0f, x2 - x1);
    float inter_h = std::max(0.0f, y2 - y1);
    float intersection = inter_w * inter_h;
    
    float union_area = a.w * a.h + b.w * b.h - intersection;
    
    if (union_area <= 0) return 0.0f;
    return intersection / union_area;
}

void YoloPoseDetector::nmsBoxes(std::vector<Box>& boxes, std::vector<float>& confidences,
                                  float scoreThreshold, float nmsThreshold, std::vector<int>& result) {
    // 建立索引和置信度的配對
    struct DetPair {
        int index;
        float conf;
        Box box;
    };
    
    std::vector<DetPair> pairs;
    for (size_t i = 0; i < boxes.size(); i++) {
        pairs.push_back({(int)i, confidences[i], boxes[i]});
    }
    
    // 按置信度排序（降序）
    std::sort(pairs.begin(), pairs.end(), [](const DetPair& a, const DetPair& b) {
        return a.conf > b.conf;
    });
    
    // NMS
    for (size_t k = 0; k < pairs.size(); k++) {
        if (pairs[k].conf < scoreThreshold) continue;
        
        result.push_back(pairs[k].index);
        
        // 移除重疊的框
        for (size_t j = k + 1; j < pairs.size(); ) {
            float iou = boxIou(pairs[k].box, pairs[j].box);
            if (iou > nmsThreshold) {
                pairs.erase(pairs.begin() + j);
            } else {
                j++;
            }
        }
    }
}

bool YoloPoseDetector::init(const void* model_data, size_t model_size) {
    printf("[YOLO] Initializing YOLOv8-Pose detector...\n");
    
    // 載入模型
    const tflite::Model* model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[YOLO] Model schema mismatch\n");
        return false;
    }
    
    // Op Resolver
    static tflite::MicroMutableOpResolver<16> micro_op_resolver;
    micro_op_resolver.AddCustom(tflite::GetString_ETHOSU(), tflite::Register_ETHOSU());
    micro_op_resolver.AddConv2D();
    micro_op_resolver.AddDepthwiseConv2D();
    micro_op_resolver.AddMaxPool2D();
    micro_op_resolver.AddAveragePool2D();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddConcatenation();
    micro_op_resolver.AddSoftmax();
    micro_op_resolver.AddQuantize();
    micro_op_resolver.AddDequantize();
    micro_op_resolver.AddAdd();
    micro_op_resolver.AddMul();
    micro_op_resolver.AddPad();
    micro_op_resolver.AddResizeNearestNeighbor();
    micro_op_resolver.AddSplit();
    micro_op_resolver.AddTranspose();
    
    // 建立 Interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena_,
        YOLO_TENSOR_ARENA_SIZE
    );
    
    auto* interpreter = &static_interpreter;
    
    // 分配 Tensors
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        printf("[YOLO] AllocateTensors failed\n");
        return false;
    }
    
    interpreter_ = (void*)interpreter;
    input_tensor_ = (void*)interpreter->input(0);
    
    auto* input = (TfLiteTensor*)input_tensor_;
    printf("[YOLO] Model loaded successfully\n");
    printf("[YOLO] Input: %d x %d x %d\n",
           input->dims->data[1], input->dims->data[2], input->dims->data[3]);
    printf("[YOLO] Num outputs: %zu\n", interpreter->outputs_size());
    
    // 初始化 anchors 和 strides
    initAnchorsAndStrides();
    
    return true;
}

void YoloPoseDetector::preprocessImage(const uint8_t* image, int width, int height) {
    auto* input = (TfLiteTensor*)input_tensor_;
    int8_t* input_data = input->data.int8;
    
    // Resize
    uint8_t* resized = new uint8_t[YOLO_INPUT_WIDTH * YOLO_INPUT_HEIGHT * 3];
    
    ImageUtils::resize(image, width, height, 
                      resized, YOLO_INPUT_WIDTH, YOLO_INPUT_HEIGHT);
    
    // 轉換為 int8 (減去 128)
    for (int i = 0; i < YOLO_INPUT_WIDTH * YOLO_INPUT_HEIGHT * 3; i++) {
        input_data[i] = (int8_t)((int32_t)resized[i] - 128);
    }
    
    delete[] resized;
}

std::vector<PersonDetection> YoloPoseDetector::parseOutput() {
    std::vector<PersonDetection> detections;
    
    auto* interpreter = (tflite::MicroInterpreter*)interpreter_;
    
    // 取得所有輸出張量
    size_t num_outputs = interpreter->outputs_size();
    if (num_outputs != NUM_OUTPUTS) {
        printf("[YOLO] Warning: Expected %d outputs, got %zu\n", NUM_OUTPUTS, num_outputs);
    }
    
    void* outputs[NUM_OUTPUTS];
    for (int i = 0; i < NUM_OUTPUTS && i < (int)num_outputs; i++) {
        outputs[i] = (void*)interpreter->output(i);
        auto* t = (TfLiteTensor*)outputs[i];
        printf("[YOLO] Output[%d] dims: ", i);
        for (int d = 0; d < t->dims->size; d++) {
            printf("%d ", t->dims->data[d]);
        }
        printf("\n");
    }
    
    // 根據實際輸出張量維度 (256x256 輸入):
    // Output[0] dims: 1 256 64   <- stride 16 的 bbox
    // Output[1] dims: 1 1024 64  <- stride 8 的 bbox
    // Output[2] dims: 1 64 1     <- stride 32 的 confidence
    // Output[3] dims: 1 1344 51  <- keypoints (所有尺度合併)
    // Output[4] dims: 1 1024 1   <- stride 8 的 confidence
    // Output[5] dims: 1 64 64    <- stride 32 的 bbox
    // Output[6] dims: 1 256 1    <- stride 16 的 confidence
    
    // 使用預先計算的尺度邊界
    int out_dim_total = total_anchors_;
    
    printf("[YOLO] Total anchors: %d\n", out_dim_total);
    printf("[YOLO] Scale boundaries: stride8=%d, stride8+16=%d, all=%d\n", 
           out_dim_size_[0], out_dim_size_[1], out_dim_size_[2]);
    
    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<Box> boxes;
    std::vector<HumanPose*> kpts_vector;
    
    // 遍歷所有候選框
    for (int dim1 = 0; dim1 < out_dim_total; dim1++) {
        // 取得置信度
        // Output[4] (1024 x 1) = stride 8 的 confidence
        // Output[6] (256 x 1) = stride 16 的 confidence
        // Output[2] (64 x 1) = stride 32 的 confidence
        float maxScore = 0.0f;
        int output_data_idx;
        int local_idx;
        
        if (dim1 < out_dim_size_[0]) {
            // stride 8
            output_data_idx = 4;
            local_idx = dim1;
        } else if (dim1 < out_dim_size_[1]) {
            // stride 16
            output_data_idx = 6;
            local_idx = dim1 - out_dim_size_[0];
        } else {
            // stride 32
            output_data_idx = 2;
            local_idx = dim1 - out_dim_size_[1];
        }
        
        maxScore = sigmoid(getBboxDequantValue(local_idx, 0, outputs[output_data_idx]));
        
        // 過濾低置信度
        if (maxScore >= MODEL_SCORE_THRESHOLD) {
            // 計算 bbox
            Box bbox;
            calculateXYWH(dim1, outputs, &bbox, out_dim_size_);
            
            // 檢查 bbox 有效性
            if (bbox.w > 0 && bbox.h > 0 && 
                bbox.x >= 0 && bbox.y >= 0 &&
                bbox.x + bbox.w <= YOLO_INPUT_WIDTH && 
                bbox.y + bbox.h <= YOLO_INPUT_HEIGHT) {
                
                boxes.push_back(bbox);
                class_ids.push_back(0);  // person class
                confidences.push_back(maxScore);
                
                // 計算 keypoints (從 Output[3])
                HumanPose* kpts = new HumanPose[NUM_KEYPOINTS];
                for (int k = 0; k < NUM_KEYPOINTS; k++) {
                    float kpt_x = getKeypointDequantValue(
                        dim1, k * 3, outputs[3],
                        anchor_array_[dim1][0], anchor_array_[dim1][1], stride_array_[dim1]);
                    float kpt_y = getKeypointDequantValue(
                        dim1, k * 3 + 1, outputs[3],
                        anchor_array_[dim1][0], anchor_array_[dim1][1], stride_array_[dim1]);
                    float kpt_score = getKeypointDequantValue(
                        dim1, k * 3 + 2, outputs[3],
                        anchor_array_[dim1][0], anchor_array_[dim1][1], stride_array_[dim1]);
                    
                    // 限制在圖像範圍內
                    if (kpt_x < 0) kpt_x = 0;
                    if (kpt_y < 0) kpt_y = 0;
                    if (kpt_x >= YOLO_INPUT_WIDTH) kpt_x = YOLO_INPUT_WIDTH - 1;
                    if (kpt_y >= YOLO_INPUT_HEIGHT) kpt_y = YOLO_INPUT_HEIGHT - 1;
                    
                    kpts[k].x = (uint32_t)kpt_x;
                    kpts[k].y = (uint32_t)kpt_y;
                    kpts[k].score = kpt_score;
                }
                kpts_vector.push_back(kpts);
            }
        }
    }
    
    printf("[YOLO] Before NMS: %zu boxes\n", boxes.size());
    
    // NMS
    std::vector<int> nms_result;
    nmsBoxes(boxes, confidences, MODEL_SCORE_THRESHOLD, MODEL_NMS_THRESHOLD, nms_result);
    
    printf("[YOLO] After NMS: %zu detections\n", nms_result.size());
    
    // 建立最終結果
    for (size_t i = 0; i < nms_result.size(); i++) {
        int idx = nms_result[i];
        PersonDetection det;
        det.bbox = boxes[idx];
        det.confidence = confidences[idx];
        
        // 複製 keypoints
        for (int k = 0; k < NUM_KEYPOINTS; k++) {
            det.keypoints[k] = kpts_vector[idx][k];
        }
        
        detections.push_back(det);
        
        printf("[YOLO] Detection %zu: conf=%.3f bbox=(%.1f, %.1f, %.1f, %.1f)\n",
               i, det.confidence, det.bbox.x, det.bbox.y, det.bbox.w, det.bbox.h);
    }
    
    // 釋放臨時 keypoints 記憶體
    for (auto* kpts : kpts_vector) {
        delete[] kpts;
    }
    
    return detections;
}

std::vector<PersonDetection> YoloPoseDetector::detect(const uint8_t* image, int width, int height) {
    auto* interpreter = (tflite::MicroInterpreter*)interpreter_;
    
    // Preprocess
    preprocessImage(image, width, height);
    
    // Inference
    uint32_t start = get_cycle_count();
    
    TfLiteStatus invoke_status = interpreter->Invoke();
    
    uint32_t end = get_cycle_count();
    float inference_ms = (end - start) / (float)(SystemCoreClock / 1000);
    
    total_inferences_++;
    total_inference_time_ += inference_ms;
    
    if (invoke_status != kTfLiteOk) {
        printf("[YOLO] Invoke failed\n");
        return std::vector<PersonDetection>();
    }
    
    // Parse output
    auto detections = parseOutput();
    
    printf("[YOLO] Detected %zu persons (%.1f ms)\n", detections.size(), inference_ms);
    
    return detections;
}

void YoloPoseDetector::printStats() const {
    if (total_inferences_ > 0) {
        printf("[YOLO] Statistics:\n");
        printf("  Total inferences: %d\n", total_inferences_);
        printf("  Average time: %.2f ms\n", total_inference_time_ / total_inferences_);
    }
}