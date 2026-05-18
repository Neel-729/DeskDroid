#include "scheduler.h"

Scheduler::Scheduler(ScheduledTask tasks[], uint8_t taskCount)
  : tasks_(tasks), taskCount_(taskCount), stats_({0, 0, 0, 0}) {}

void Scheduler::reset(unsigned long now){
  stats_ = {0, 0, 0, 0};
  for(uint8_t i=0;i<taskCount_;i++){
    tasks_[i].lastRunMs = now;
    tasks_[i].runCount = 0;
    tasks_[i].maxRuntimeUs = 0;
    tasks_[i].overrunCount = 0;
  }
}

void Scheduler::run(unsigned long now){
  const unsigned long loopStartUs = micros();
  FrameContext context = {now};

  for(uint8_t i=0;i<taskCount_;i++){
    ScheduledTask &task = tasks_[i];
    if(!task.enabled) continue;
    if(task.intervalMs != 0 && now - task.lastRunMs < task.intervalMs) continue;

    const unsigned long taskStartUs = micros();
    task.lastRunMs = now;
    task.callback(context);
    const unsigned long runtimeUs = micros() - taskStartUs;

    task.runCount++;
    stats_.taskRunCount++;
    if(runtimeUs > task.maxRuntimeUs) task.maxRuntimeUs = runtimeUs;
    if(task.budgetUs != 0 && runtimeUs > task.budgetUs){
      task.overrunCount++;
      stats_.overrunCount++;
    }
  }

  const unsigned long loopRuntimeUs = micros() - loopStartUs;
  stats_.loopCount++;
  if(loopRuntimeUs > stats_.maxLoopRuntimeUs){
    stats_.maxLoopRuntimeUs = loopRuntimeUs;
  }
}

uint8_t Scheduler::taskCount() const{
  return taskCount_;
}

const SchedulerStats &Scheduler::stats() const{
  return stats_;
}
