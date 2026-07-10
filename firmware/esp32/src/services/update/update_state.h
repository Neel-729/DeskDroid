#pragma once

#include <Arduino.h>

enum class UpdateState {
    Idle,
    Checking,
    Downloading,
    Verifying,
    Installing,
    PendingReboot,
    Success,
    Failed
};