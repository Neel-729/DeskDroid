#pragma once

#include "../core/events.h"

namespace ProtocolService {

void begin();
void update();
void handleEvent(const AppEvent &event);
void requestStateSync();
bool connected();

}  // namespace ProtocolService
