#include "scheduler.h"

#include <string.h>

#include "logging.h"

namespace {
RateLimitedLog overrunLog;
}

Scheduler::Scheduler(ScheduledTask *tasks, uint8_t taskCount)
  : _tasks(tasks), _taskCount(taskCount), _stats{0, 0, 0, 0} {
}

void Scheduler::reset(unsigned long now){
  _stats = {0, 0, 0, 0};
  for(uint8_t i=0;i<_taskCount;i++){
    _tasks[i].lastRunMs = now;
    _tasks[i].lastRuntimeUs = 0;
    _tasks[i].runCount = 0;
    _tasks[i].overrunCount = 0;
  }
}

void Scheduler::run(unsigned long now){
  const uint32_t loopStart = micros();
  _stats.loopCount++;

  for(uint8_t i=0;i<_taskCount;i++){
    ScheduledTask &task = _tasks[i];
    if(!task.enabled || task.callback == nullptr) continue;
    if(task.intervalMs > 0 && now - task.lastRunMs < task.intervalMs) continue;

    FrameContext context = {
      now,
      now - task.lastRunMs,
      _stats.loopCount,
      _stats.taskRunCount,
      task.name
    };

    task.lastRunMs = now;
    const uint32_t start = micros();
    task.callback(context);
    const uint32_t runtime = micros() - start;

    task.lastRuntimeUs = runtime;
    task.runCount++;
    _stats.taskRunCount++;

    if(task.maxRuntimeUs > 0 && runtime > task.maxRuntimeUs){
      task.overrunCount++;
      _stats.overrunCount++;
      if(Log::shouldLog(overrunLog, 2000, now)){
        LOG_WARN(LogTag::SCHED, "%s overrun %lu us > %lu us", task.name, (unsigned long)runtime, (unsigned long)task.maxRuntimeUs);
      }
    }
  }

  const uint32_t loopRuntime = micros() - loopStart;
  if(loopRuntime > _stats.maxLoopRuntimeUs){
    _stats.maxLoopRuntimeUs = loopRuntime;
  }
}

bool Scheduler::setEnabled(const char *name, bool enabled){
  for(uint8_t i=0;i<_taskCount;i++){
    if(strcmp(_tasks[i].name, name) == 0){
      _tasks[i].enabled = enabled;
      return true;
    }
  }
  return false;
}

const SchedulerStats &Scheduler::stats() const {
  return _stats;
}

const ScheduledTask *Scheduler::task(uint8_t index) const {
  if(index >= _taskCount) return nullptr;
  return &_tasks[index];
}

uint8_t Scheduler::taskCount() const {
  return _taskCount;
}
