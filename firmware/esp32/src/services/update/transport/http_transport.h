#pragma once

#include <Arduino.h>
#include "itransport.h"

namespace Transport {

class HttpTransport : public ITransport {
public:
    /**
     * @brief Construct a new Http Transport
     */
    HttpTransport();
    
    /**
     * @brief Destroy the Http Transport object
     */
    virtual ~HttpTransport() override;
    
    // ITransport interface implementation
    virtual bool initialize() override;
    virtual void shutdown() override;
    virtual bool open(const TransportRequest& request, TransportResponse& response) override;
    virtual void close(TransportResult& result) override;
    virtual void cancel() override;
    virtual bool isBusy() const override;
    virtual bool supportsResume() const override;
    virtual bool supportsStreaming() const override;
    virtual TransportState state() const override;
    virtual const TransportProgress& progress() const override;
    virtual const TransportCapabilities& capabilities() const override;
    virtual TransportError lastError() const override;
    virtual const char* transportName() const override;
    virtual size_t read(uint8_t* buffer, size_t maxBytes) override;
    
    /**
     * @brief Set the timeout for HTTP operations
     * @param timeoutMs Timeout in milliseconds
     */
    void setTimeout(uint32_t timeoutMs);
    
    /**
     * @brief Get the current timeout setting
     * @return Timeout in milliseconds
     */
    uint32_t getTimeout() const;
    
private:
    // Internal state management
    void transitionTo(TransportState newState);
    bool isValidTransition(TransportState from, TransportState to) const;
    
    bool m_initialized;
    TransportState m_currentState;
    TransportError m_lastError;
    TransportProgress m_progress;
    TransportCapabilities m_capabilities;
    uint32_t m_timeoutMs;
    
    // Request tracking - only store what's necessary
    TransportRequest m_currentRequest;
    uint32_t m_transferStartTime;
    size_t m_remainingBytes;
};

} // namespace Transport