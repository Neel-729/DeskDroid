# Verification Layer Example Usage

## Phase 5 Implementation Example

This example demonstrates how to use the new Verification Layer in your firmware code.

### 1. Create and register a verifier with UpdateManager

```cpp
#include "services/update/verification/update_verifier.h"

// In your setup code or wherever OTA is initialized
void setupOTA() {
    // Create a verifier instance
    static Verification::UpdateVerifier verifier;
    
    // Configure policy based on build configuration
#ifdef DEBUG_BUILD
    // Use development policy for debugging
    auto policy = Verification::VerificationPolicy::createDevelopmentPolicy();
    policy.allowDowngrades = true;
    verifier.setPolicy(policy);
#else
    // Use strict production policy for release builds
    auto policy = Verification::VerificationPolicy::createProductionPolicy();
    
    // Define allowed hardware variants
    const char* allowedHardware[] = {"deskdroid-v1", "deskdroid-v2"};
    policy.allowedHardwareVariants = allowedHardware;
    policy.allowedVariantsCount = 2;
    policy.maxPayloadSizeBytes = 1024 * 1024 * 4; // 4MB limit
    verifier.setPolicy(policy);
#endif
    
    // Register verifier with UpdateManager
    UpdateManager::registerVerifier(&verifier);
    
    // Begin the OTA system
    UpdateManager::begin();
}
```

### 2. Perform verification after transport completes

```cpp
// After transport has downloaded an update, verify it before passing to installer
void onDownloadComplete(const Transport::TransportResult& result) {
    auto* verifier = UpdateManager::currentVerifier();
    if (!verifier || !verifier->isReady()) {
        ESP_LOGE("OTA", "No verifier available, cannot proceed with installation");
        return;
    }
    
    // Build verification context
    Verification::VerificationContext ctx;
    ctx.currentFirmware = &UpdateManager::firmwareInfo();
    ctx.candidateUpdate = &UpdateManager::getLatestUpdateInfo();
    
    // Describe the payload we received
    Verification::PayloadDescriptor payload;
    payload.data = result.data;
    payload.actualSize = result.bytesReceived;
    payload.declaredSize = UpdateManager::getLatestUpdateInfo().fileSize;
    payload.expectedHash = UpdateManager::getLatestUpdateInfo().sha256.c_str();
    payload.isComplete = true;
    ctx.payload = &payload;
    
    // Use the verifier's policy
    const auto& activePolicy = static_cast<const Verification::UpdateVerifier*>(verifier)->getPolicy();
    ctx.policy = &activePolicy;
    
    // Run verification
    Verification::VerificationResult verification = verifier->verify(ctx);
    
    if (verification.isSuccess() && verification.eligibleForInstallation) {
        ESP_LOGI("OTA", "Verification passed! Proceeding to installation...");
        // Pass verified artifact to installer - installer only accepts VerifiedArtifact
        // installer.install(verificationResult.verifiedArtifact);
    } else {
        ESP_LOGE("OTA", "Verification failed! Status: %d, Error: %d", 
                 static_cast<int>(verification.status), 
                 static_cast<int>(verification.error));
        ESP_LOGE("OTA", "Diagnostic: %s", verification.details.diagnosticMessage);
    }
}
```

## Architecture Overview

```
Transport Layer -> Verification Layer -> Installer Layer
      (downloads)        (validates)        (flashes)
```

The verification layer maintains strict separation:
- **Transport** only handles getting bytes from source to device
- **Verification** only evaluates if bytes are safe to install
- **Installer** only writes bytes to flash (never gets unverified data)

## Future Extensibility

In future phases, this architecture will support:
- SHA-256 hash verification in the integrity stage
- ECDSA or RSA signature verification in the authenticity stage
- Certificate chain validation
- Rollback protection
- Hardware whitelisting enforcement
- Telemetry and audit logging