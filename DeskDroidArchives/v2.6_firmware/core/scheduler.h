#pragma once

#include <Arduino.h>

struct FrameContext {
  unsigned long nowMs;
};

using ScheduledTaskCallback = void (*)(FrameContext &context);

struct ScheduledTask {
  const char* name;
  unsigned long intervalMs;
  unsigned long lastRunMs;
  unsigned long runCount;
  unsigned long budgetUs;
  unsigned long maxRuntimeUs;
  uint16_t overrunCount;
  bool enabled;
  ScheduledTaskCallback callback;
};

struct SchedulerStats {
  uint32_t loopCount;
  uint32_t taskRunCount;
  uint16_t overrunCount;
  unsigned long maxLoopRuntimeUs;
};

class Scheduler {
 public:
  Scheduler(ScheduledTask tasks[], uint8_t taskCount);

  void reset(unsigned long now);
  void run(unsigned long now);
  uint8_t taskCount() const;
  const SchedulerStats &stats() const;

 private:
  ScheduledTask* tasks_;
  uint8_t taskCount_;
  SchedulerStats stats_;
};
