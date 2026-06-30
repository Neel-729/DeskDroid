#pragma once

namespace Version {

// Product Information
constexpr const char* FirmwareName        = "DeskDroid";
constexpr const char* FirmwareVersion     = "2.6.8";
constexpr const char* FirmwareChannel     = "Development";   // Development, Beta, Stable

// Build Information
constexpr const char* BuildDate           = __DATE__;
constexpr const char* BuildTime           = __TIME__;

// Hardware Compatibility
constexpr const char* HardwareRevision    = "Rev A";
constexpr const char* ProtocolVersion     = "1.0";

// OTA Information
constexpr bool OTAEnabled                 = true;
constexpr const char* FirmwareVersionCode    = "20608";   // 2.6.8 -> 20608

}