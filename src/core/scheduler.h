#pragma once

#include <Arduino.h>

struct FrameContext {
  unsigned long nowMs;
  unsigned long deltaMs;
  uint32_t loopCount;
  uint32_t taskRunCount;
  const char *taskName;
};

typedef void (*TaskCallback)(FrameContext &context);

struct ScheduledTask {
  const char *name;
  unsigned long intervalMs;
  unsigned long lastRunMs;
  uint32_t lastRuntimeUs;
  uint32_t maxRuntimeUs;
  uint32_t runCount;
  uint16_t overrunCount;
  bool enabled;
  TaskCallback callback;
};

struct SchedulerStats {
  uint32_t loopCount;
  uint32_t taskRunCount;
  uint16_t overrunCount;
  uint32_t maxLoopRuntimeUs;
};

class Scheduler {
public:
  Scheduler(ScheduledTask *tasks, uint8_t taskCount);

  void reset(unsigned long now);
  void run(unsigned long now);

  bool setEnabled(const char *name, bool enabled);
  const SchedulerStats &stats() const;
  const ScheduledTask *task(uint8_t index) const;
  uint8_t taskCount() const;

private:
  ScheduledTask *_tasks;
  uint8_t _taskCount;
  SchedulerStats _stats;
};
