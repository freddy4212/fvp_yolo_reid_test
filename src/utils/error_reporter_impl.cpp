// ErrorReporter implementation for TFLite Micro
#include "tensorflow/lite/core/api/error_reporter.h"
#include <cstdarg>
#include <cstdio>

namespace tflite {

// Provide the Report method implementation
int ErrorReporter::Report(const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = vprintf(format, args);
  va_end(args);
  return result;
}

}  // namespace tflite
