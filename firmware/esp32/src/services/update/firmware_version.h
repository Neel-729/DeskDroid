#pragma once

#include <stdint.h>

// Canonical firmware version definition - single source of truth
#define FIRMWARE_VERSION_MAJOR 2
#define FIRMWARE_VERSION_MINOR 6
#define FIRMWARE_VERSION_PATCH 8

// String representation of the semantic version
#define FIRMWARE_VERSION_STRING "2.6.8"

// Calculate deterministic version code: major * 10000 + minor * 100 + patch
// This allows for up to 99 major, 99 minor, 99 patch versions
#define FIRMWARE_VERSION_CODE (FIRMWARE_VERSION_MAJOR * 10000 + FIRMWARE_VERSION_MINOR * 100 + FIRMWARE_VERSION_PATCH)

// Hardware and protocol versions (can be moved to build config if needed)
#define HARDWARE_REVISION "1.0"
#define PROTOCOL_VERSION 1