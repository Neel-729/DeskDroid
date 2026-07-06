#pragma once

#include <Arduino.h>
#include "transport_types.h"

namespace Transport {

class ITransport {
public:
    virtual ~ITransport() = default;
    
    /**
     * @brief Initialize the transport
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shutdown the transport, release resources
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Open a new transfer with the given request
     * @param request The transport request
     * @param response Output parameter for the response
     * @return true if the transfer was successfully started
     */
    virtual bool open(const TransportRequest& request, TransportResponse& response) = 0;
    
    /**
     * @brief Close the current transfer
     * @param result Output parameter for the final result
     */
    virtual void close(TransportResult& result) = 0;
    
    /**
     * @brief Cancel the current active transfer
     */
    virtual void cancel() = 0;
    
    /**
     * @brief Check if transport is busy with an active transfer
     * @return true if busy
     */
    virtual bool isBusy() const = 0;
    
    /**
     * @brief Check if this transport supports resume functionality
     * @return true if resume is supported
     */
    virtual bool supportsResume() const = 0;
    
    /**
     * @brief Check if this transport supports streaming
     * @return true if streaming is supported
     */
    virtual bool supportsStreaming() const = 0;
    
    /**
     * @brief Get the current transport state
     * @return Current TransportState
     */
    virtual TransportState state() const = 0;
    
    /**
     * @brief Get the current progress of the active transfer
     * @return Const reference to current progress
     */
    virtual const TransportProgress& progress() const = 0;
    
    /**
     * @brief Get the capabilities of this transport
     * @return Const reference to TransportCapabilities
     */
    virtual const TransportCapabilities& capabilities() const = 0;
    
    /**
     * @brief Get the last error that occurred
     * @return Last TransportError
     */
    virtual TransportError lastError() const = 0;
    
    /**
     * @brief Get the transport name for debugging
     * @return String identifier of the transport type
     */
    virtual const char* transportName() const = 0;
    
    /**
     * @brief Read data from the current transfer
     * @param buffer Destination buffer
     * @param maxBytes Maximum bytes to read
     * @return Number of bytes actually read, 0 on error/end
     */
    virtual size_t read(uint8_t* buffer, size_t maxBytes) = 0;
};

} // namespace Transport