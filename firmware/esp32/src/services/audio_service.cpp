#include "audio_service.h"

#include "../app/hardware_requests.h"
#include "../core/system_state.h"

namespace AudioService {

void service(){
  HardwareRequests::serviceBuzzer();
}

void beep(uint16_t durationMs){
  const AudioState &audio = SystemStateStore::current().audio;
  if(audio.muted || !audio.buzzerEnabled || audio.volume == 0) return;
  HardwareRequests::requestBeep(durationMs, CommandSource::APP);
}

}  // namespace AudioService
