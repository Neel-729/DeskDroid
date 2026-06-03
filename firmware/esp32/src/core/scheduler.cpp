#include "scheduler.h"

#include "fault_tracker.h"

Scheduler::Scheduler(ScheduledTask tasks[], uint8_t taskCount)
  : tasks_(tasks), taskCount_(taskCount), stats_({0, 0, 0, 0, nullptr, 0, 0, 0}) {}

void Scheduler::reset(unsigned long now){
  stats_ = {0, 0, 0, 0, nullptr, 0, 0, 0};
  for(uint8_t i=0;i<taskCount_;i++){
    tasks_[i].lastRunMs = now;
    tasks_[i].runCount = 0;
    tasks_[i].maxRuntimeUs = 0;
    tasks_[i].overrunCount = 0;
    tasks_[i].lastRuntimeUs = 0;
    tasks_[i].totalRuntimeUs = 0;
  }
}

void Scheduler::run(unsigned long now){
  const unsigned long loopStartUs = micros();
  FrameContext context = {now};

  for(uint8_t i=0;i<taskCount_;i++){
    ScheduledTask &task = tasks_[i];
    if(!task.enabled) continue;
    if(task.intervalMs != 0 && now - task.lastRunMs < task.intervalMs) continue;
    if(task.callback == nullptr){
      FaultTracker::record(FaultSource::Scheduler, FaultCode::SchedulerInvalidTask, task.name, now);
      continue;
    }

    const unsigned long taskStartUs = micros();
    task.lastRunMs = now;
    task.callback(context);
    const unsigned long runtimeUs = micros() - taskStartUs;

    task.runCount++;
    stats_.taskRunCount++;
    task.lastRuntimeUs = runtimeUs;
    task.totalRuntimeUs += runtimeUs;
    if(runtimeUs > task.maxRuntimeUs) task.maxRuntimeUs = runtimeUs;
    if(task.budgetUs != 0 && runtimeUs > task.budgetUs){
      task.overrunCount++;
      stats_.overrunCount++;
      stats_.lastOverrunTaskName = task.name;
      stats_.lastOverrunRuntimeUs = runtimeUs;
      stats_.lastOverrunBudgetUs = task.budgetUs;
      stats_.lastOverrunTimestampMs = now;
      FaultTracker::record(FaultSource::Scheduler, FaultCode::SchedulerOverrun, task.name, now);
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

const ScheduledTask& Scheduler::taskAt(uint8_t index) const{
  if(index >= taskCount_) return tasks_[0];
  return tasks_[index];
}

const SchedulerStats &Scheduler::stats() const{
  return stats_;
}
