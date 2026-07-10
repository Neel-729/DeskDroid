#pragma once

#include <Arduino.h>
#include "installer_types.h"

namespace Installation {

/**
 * @brief Interface for all installer implementations
 * 
 * Defines the minimal public API that all installers must implement.
 * The installer is responsible ONLY for the installation lifecycle of
 * an already verified firmware payload.
 */
class IInstaller {
public:
    virtual ~IInstaller() = default;

    /**
     * @brief Initialize the installer - allocate resources, prepare state
     * @return true if initialization succeeded
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the installer - clean up resources, reset state
     */
    virtual void shutdown() = 0;

    /**
     * @brief Begin a new installation with the provided context
     * @param context Complete installation context
     * @return true if installation started successfully
     */
    virtual bool beginInstallation(const InstallationContext& context) = 0;

    /**
     * @brief Write a chunk of firmware data
     * @param data Pointer to data chunk
     * @param length Length of data chunk in bytes
     * @return true if chunk was accepted and written
     */
    virtual bool write(const void* data, size_t length) = 0;

    /**
     * @brief Finalize the installation - complete all writes and close the OTA transaction
     * @return InstallationResult with the final outcome
     */
    virtual InstallationResult finalizeInstallation() = 0;

    /**
     * @brief Activate an already-installed firmware as the next boot partition
     * @return ActivationResult with the activation outcome
     */
    virtual ActivationResult activateInstalledFirmware() = 0;

    /**
     * @brief Abort the current installation immediately
     */
    virtual void abort() = 0;

    /**
     * @brief Reset the installer state to idle, clean up any ongoing installation
     */
    virtual void reset() = 0;

    /**
     * @brief Get the current state of the installer
     * @return Current InstallationState
     */
    virtual InstallationState state() const = 0;

    /**
     * @brief Get the current progress of the active installation
     * @return const InstallationProgress&
     */
    virtual const InstallationProgress& progress() const = 0;

    /**
     * @brief Get the capabilities of this installer implementation
     * @return const InstallationCapabilities&
     */
    virtual const InstallationCapabilities& capabilities() const = 0;

    /**
     * @brief Get the last error that occurred
     * @return Last InstallationError
     */
    virtual InstallationError lastError() const = 0;

    /**
     * @brief Get the result of the last completed installation
     * @return const InstallationResult&
     */
    virtual const InstallationResult& lastResult() const = 0;

    /**
     * @brief Check if the installer is busy with an active installation
     * @return true if installation is in progress
     */
    virtual bool isBusy() const = 0;

    /**
     * @brief Check if the installer is initialized and ready for use
     * @return true if ready
     */
    virtual bool isReady() const = 0;
};

} // namespace Installation
