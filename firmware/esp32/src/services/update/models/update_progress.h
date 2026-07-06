#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Progress tracking structure for download and installation phases
 * 
 * Provides real-time progress information for long-running update operations,
 * supporting both download and installation phases with detailed byte-level
 * and percentage tracking.
 */
struct UpdateProgress {
    /**
     * @brief Current phase of the update operation
     */
    enum class Phase {
        Idle,           ///< No operation in progress
        Downloading,    ///< Firmware download in progress
        Verifying,      ///< Downloaded image verification in progress
        PreparingFlash, ///< Flash preparation in progress
        Writing,        ///< Flash write operation in progress
        Finalizing,     ///< Post-flash finalization
        Completed,      ///< Operation completed successfully
        Failed          ///< Operation failed
    };
    
    /**
     * @brief Current operation phase
     */
    Phase phase = Phase::Idle;
    
    /**
     * @brief Number of bytes processed in current phase
     */
    size_t bytesProcessed = 0;
    
    /**
     * @brief Total bytes expected for current phase
     */
    size_t bytesTotal = 0;
    
    /**
     * @brief Timestamp when current phase started (millis())
     */
    uint32_t phaseStartTime = 0;
    
    /**
     * @brief Calculate percentage complete for current phase
     * @return Percentage (0-100), or 0 if no bytes to process
     */
    uint8_t percentComplete() const {
        if (bytesTotal == 0) return 0;
        return static_cast<uint8_t>((bytesProcessed * 100) / bytesTotal);
    }
    
    /**
     * @brief Check if an operation is currently in progress
     * @return true if update is active
     */
    bool isInProgress() const {
        return phase != Phase::Idle && 
               phase != Phase::Completed && 
               phase != Phase::Failed;
    }
    
    /**
     * @brief Reset all progress values to idle state
     */
    void reset() {
        phase = Phase::Idle;
        bytesProcessed = 0;
        bytesTotal = 0;
        phaseStartTime = 0;
    }
    
    /**
     * @brief Start a new phase with the given total byte count
     * @param newPhase The phase to enter
     * @param totalBytes Total bytes expected for this phase
     */
    void startPhase(Phase newPhase, size_t totalBytes = 0) {
        phase = newPhase;
        bytesProcessed = 0;
        bytesTotal = totalBytes;
        phaseStartTime = millis();
    }
    
    /**
     * @brief Update the number of processed bytes
     * @param processed Number of bytes completed
     */
    void updateProgress(size_t processed) {
        bytesProcessed = processed;
    }
    
    /**
     * @brief Mark current phase as completed
     */
    void complete() {
        phase = Phase::Completed;
        bytesProcessed = bytesTotal;
    }
    
    /**
     * @brief Mark operation as failed
     */
    void markFailed() {
        phase = Phase::Failed;
    }
};