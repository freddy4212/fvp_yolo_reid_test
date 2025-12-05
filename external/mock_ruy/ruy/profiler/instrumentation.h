#ifndef RUY_PROFILER_INSTRUMENTATION_H_
#define RUY_PROFILER_INSTRUMENTATION_H_

#define RUY_PROFILE_SCOPE(name)

namespace ruy {
namespace profiler {
  // Mock ScopeLabel class to satisfy usage like: ruy::profiler::ScopeLabel label("name");
  class ScopeLabel {
  public:
    explicit ScopeLabel(const char*) {}
  };
}
}

#endif  // RUY_PROFILER_INSTRUMENTATION_H_