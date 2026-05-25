#include "hardware_requests.h"

#include "../core/logging.h"
#include "../core/system_state.h"
#include "../drivers/buzzer_driver.h"
#include "../drivers/lcd_driver.h"

namespace {

constexpr uint8_t COMMAND_QUEUE_SIZE = 16;

HardwareCommand commandQueue[COMMAND_QUEUE_SIZE];
uint8_t commandHead = 0;
uint8_t commandTail = 0;
HardwareRequestStats requestStats = {0, 0, 0, 0, 0, 0};
RateLimitedLog droppedCommandLog;
uint16_t nextSequenceId = 1;

uint8_t queueDepth(){
  if(commandHead >= commandTail) return commandHead - commandTail;
  return COMMAND_QUEUE_SIZE - commandTail + commandHead;
}

bool dequeue(HardwareCommand &command){
  if(commandTail == commandHead) return false;

  command = commandQueue[commandTail];
  commandTail = (commandTail + 1) % COMMAND_QUEUE_SIZE;
  return true;
}

const char* sourceName(CommandSource source){
  switch(source){
    case CommandSource::SYSTEM: return "SYSTEM";
    case CommandSource::APP: return "APP";
    case CommandSource::UI: return "UI";
    case CommandSource::TIMER: return "TIMER";
    case CommandSource::REMINDER: return "REMINDER";
    case CommandSource::SETTINGS: return "SETTINGS";
    case CommandSource::LIGHTING: return "LIGHTING";
    case CommandSource::INPUTS: return "INPUT";
  }
  return "?";
}

void execute(const HardwareCommand &command){
  switch(command.type){
    case HardwareCommandType::BEEP:
      BuzzerDriver::trigger(command.value);
      break;

    case HardwareCommandType::LED_MODE:
      SystemStateStore::setLedMode((LedState)command.value);
      LOG_INFO(
        LogTag::LED,
        "Mode requested %u seq=%u src=%s age=%lums",
        (unsigned)command.value,
        command.sequenceId,
        sourceName(command.source),
        (unsigned long)(millis() - command.timestamp)
      );
      break;

    case HardwareCommandType::IDLE_PRESET:
      SystemStateStore::setIdlePreset((LedIdlePreset)command.value);
      break;

    case HardwareCommandType::LED_BRIGHTNESS:
      SystemStateStore::setBrightnessLevel((uint8_t)command.value);
      break;

    case HardwareCommandType::BACKLIGHT:
      LcdDriver::setBacklight(command.value != 0);
      break;
  }
}

}

namespace HardwareRequests {

void beginLocal(const DeviceSettings &settings){
  LcdDriver::begin();
  LcdDriver::createBlockChar();
  BuzzerDriver::begin();
  (void)settings;
}

void requestBeep(uint16_t durationMs, CommandSource source){
  enqueue({HardwareCommandType::BEEP, durationMs, 0, 0, source});
}

void requestLedMode(LedState mode, CommandSource source){
  enqueue({HardwareCommandType::LED_MODE, (uint16_t)mode, 0, 0, source});
}

void requestIdlePreset(LedIdlePreset preset, CommandSource source){
  enqueue({HardwareCommandType::IDLE_PRESET, (uint16_t)preset, 0, 0, source});
}

void requestLedBrightness(uint8_t level, CommandSource source){
  enqueue({HardwareCommandType::LED_BRIGHTNESS, level, 0, 0, source});
}

void requestBacklight(bool enabled, CommandSource source){
  enqueue({HardwareCommandType::BACKLIGHT, (uint16_t)(enabled ? 1U : 0U), 0, 0, source});
}

bool enqueue(const HardwareCommand &command){
  uint8_t nextHead = (commandHead + 1) % COMMAND_QUEUE_SIZE;
  if(nextHead == commandTail){
    requestStats.dropped++;
    if(Log::shouldLog(droppedCommandLog, 2000, millis())){
      LOG_WARN(LogTag::APP, "Hardware command queue full, dropped=%u", requestStats.dropped);
    }
    return false;
  }

  HardwareCommand stampedCommand = command;
  stampedCommand.timestamp = millis();
  stampedCommand.sequenceId = nextSequenceId++;
  if(nextSequenceId == 0) nextSequenceId = 1;

  commandQueue[commandHead] = stampedCommand;
  commandHead = nextHead;
  requestStats.queued++;
  requestStats.lastSequenceId = stampedCommand.sequenceId;

  const uint8_t depth = queueDepth();
  if(depth > requestStats.maxDepth){
    requestStats.maxDepth = depth;
  }
  return true;
}

void executePending(){
  HardwareCommand command;
  while(dequeue(command)){
    uint32_t age = millis() - command.timestamp;
    if(age > requestStats.maxCommandAgeMs){
      requestStats.maxCommandAgeMs = age;
    }
    execute(command);
    requestStats.executed++;
  }
}

const HardwareRequestStats &stats(){
  return requestStats;
}

void serviceBuzzer(){
  BuzzerDriver::update();
}

void updateLeds(bool lightsAllowed){
  SystemStateStore::setLightingScheduleAllowed(lightsAllowed);
}

void clearDisplay(){
  LcdDriver::clear();
}

void writeDisplayRows(const char* row0, const char* row1){
  LcdDriver::writeRow(0, row0);
  LcdDriver::writeRow(1, row1);
}

}
