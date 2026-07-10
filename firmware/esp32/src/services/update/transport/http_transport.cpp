#include "http_transport.h"
#include "../../../core/logging.h"

namespace Transport {

HttpTransport::HttpTransport()
    : m_initialized(false)
    , m_currentState(TransportState::Idle)
    , m_lastError(TransportError::None)
    , m_timeoutMs(30000) // 30 second default timeout
    , m_transferStartTime(0)
    , m_remainingBytes(0)
{
    // Honest capabilities - stub transport is a skeleton, real implementation would set these appropriately
    // Current stub implementation supports NO actual transfer capabilities
    m_capabilities.supportsResume = false;
    m_capabilities.supportsStreaming = false;
    m_capabilities.supportsSeek = false;
    m_capabilities.supportsParallel = false;
    m_capabilities.hasSecureTransport = false;
}

HttpTransport::~HttpTransport() {
    if (m_initialized) {
        shutdown();
    }
}

bool HttpTransport::initialize() {
    // Handle lifecycle sequence: Initialize → Shutdown → Initialize safely
    // Can initialize from Idle or Closed states (supports re-initialization after shutdown)
    if (m_currentState != TransportState::Idle && m_currentState != TransportState::Closed) {
        m_lastError = TransportError::InvalidRequest;
        return false;
    }
    
    // Reset ALL state before initialization - full clean slate
    m_initialized = false;
    m_lastError = TransportError::None;
    m_remainingBytes = 0;
    m_transferStartTime = 0;
    m_progress.reset();
    m_currentRequest = TransportRequest();
    
    transitionTo(TransportState::Initializing);
    
    // Stub implementation - initialization always succeeds for skeleton
    m_initialized = true;
    m_lastError = TransportError::None;
    
    transitionTo(TransportState::Ready);
    
    return true;
}

void HttpTransport::shutdown() {
    // Always cancel any active transfer first - cancel is idempotent and safe from any state
    cancel();
    
    // Reset ALL state completely - even beyond what cancel() does
    m_initialized = false;
    m_lastError = TransportError::None;
    // cancel() already reset these, but do it again to be absolutely thorough
    m_remainingBytes = 0;
    m_transferStartTime = 0;
    m_progress.reset();
    m_currentRequest = TransportRequest();
    
    // Transition to Closed state - after shutdown, only initialize() can bring us back
    transitionTo(TransportState::Closed);
}

bool HttpTransport::open(const TransportRequest& request, TransportResponse& response) {
    // Reset response
    response = TransportResponse();
    
    // Validate state
    if (!m_initialized || m_currentState != TransportState::Ready) {
        response.success = false;
        response.error = m_initialized ? TransportError::Busy : TransportError::NotInitialized;
        m_lastError = response.error;
        return false;
    }
    
    // Validate request
    if (request.uri == nullptr || *request.uri == '\0') {
        response.success = false;
        response.error = TransportError::InvalidRequest;
        m_lastError = response.error;
        return false;
    }
    
    // This is a stub implementation - no real HTTP backend exists
    // Honest response: this transport doesn't actually support transfers yet
    // Stay in Ready state - Error is only for unrecoverable failures, not "operation not supported"
    response.success = false;
    response.error = TransportError::Unsupported;
    response.httpStatusCode = 501; // Not Implemented
    m_lastError = TransportError::Unsupported;
    
    return false;
}

void HttpTransport::close(TransportResult& result) {
    // Reset result to clean state
    result = TransportResult();
    
    uint32_t transferDuration = m_transferStartTime > 0 ? (millis() - m_transferStartTime) : 0;
    
    // Always clean up all transfer-specific state, regardless of current state
    // This ensures close() is idempotent and can be called safely from any state
    m_remainingBytes = 0;
    m_transferStartTime = 0;
    m_progress.reset();
    m_currentRequest = TransportRequest();
    
    // Only attempt state transitions if we were actually in an active transfer
    if (m_currentState == TransportState::Streaming || m_currentState == TransportState::Opening || m_currentState == TransportState::Error) {
        if (m_currentState == TransportState::Streaming) {
            transitionTo(TransportState::Completed);
            result.completed = true;
            result.error = TransportError::None;
            result.totalBytesTransferred = 0; // Stub never transferred any bytes
            result.totalDuration = transferDuration;
        } else {
            // For other states, just report failure but clean up
            result.completed = false;
            result.error = m_lastError;
        }
    } else {
        // If we weren't in an active transfer, still reset everything
        result.completed = false;
        result.error = TransportError::InvalidRequest;
    }
    
    // Always return to Ready state if not closed
    if (m_currentState != TransportState::Closed) {
        transitionTo(TransportState::Ready);
    }
}

void HttpTransport::cancel() {
    // Cancel can be called from ANY state - always safe, idempotent
    // If we're actively transferring, perform cancellation sequence
    if (m_currentState == TransportState::Streaming || m_currentState == TransportState::Opening) {
        transitionTo(TransportState::Cancelled);
        m_lastError = TransportError::Cancelled;
    }
    
    // Always reset ALL transfer-specific state regardless of current state
    // This ensures cancel() is idempotent and safe to call repeatedly
    m_remainingBytes = 0;
    m_transferStartTime = 0;
    m_progress.reset();
    m_currentRequest = TransportRequest(); // Clear any stored request
    
    // Return to Ready if not closed, otherwise maintain Closed state
    if (m_currentState != TransportState::Closed) {
        transitionTo(TransportState::Ready);
    }
}

bool HttpTransport::isBusy() const {
    return m_currentState == TransportState::Opening || 
           m_currentState == TransportState::Streaming;
}

bool HttpTransport::supportsResume() const {
    return m_capabilities.supportsResume;
}

bool HttpTransport::supportsStreaming() const {
    return m_capabilities.supportsStreaming;
}

TransportState HttpTransport::state() const {
    return m_currentState;
}

const TransportProgress& HttpTransport::progress() const {
    return m_progress;
}

const TransportCapabilities& HttpTransport::capabilities() const {
    return m_capabilities;
}

TransportError HttpTransport::lastError() const {
    return m_lastError;
}

const char* HttpTransport::transportName() const {
    return "HttpTransport";
}

size_t HttpTransport::read(uint8_t* buffer, size_t maxBytes) {
    if (buffer == nullptr || maxBytes == 0) {
        m_lastError = TransportError::InvalidRequest;
        return 0;
    }
    
    // This stub transport never enters streaming state - all read attempts fail
    if (m_currentState != TransportState::Streaming) {
        m_lastError = TransportError::InvalidRequest;
        return 0;
    }
    
    // Should never reach here in stub implementation - no bytes can be read
    m_lastError = TransportError::Unsupported;
    return 0;
}

void HttpTransport::setTimeout(uint32_t timeoutMs) {
    m_timeoutMs = timeoutMs;
}

uint32_t HttpTransport::getTimeout() const {
    return m_timeoutMs;
}

bool HttpTransport::isValidTransition(TransportState from, TransportState to) const {
    // Define valid state transitions for all transport types
    // State machine graph:
    // Idle → Initializing → Ready → Opening → Streaming → Completed → Ready
    // Any state can transition to Error → Ready/Idle
    // Any state can transition to Cancelled → Ready
    // Any state can transition to Closed (shutdown)
    // No-op transitions (same state) are always allowed
    
    if (from == to) {
        return true; // No-op transitions are always safe
    }
    
    switch(from) {
        case TransportState::Idle:
            return to == TransportState::Initializing || to == TransportState::Closed || to == TransportState::Error;
            
        case TransportState::Initializing:
            return to == TransportState::Ready || to == TransportState::Error || to == TransportState::Closed;
            
        case TransportState::Ready:
            return to == TransportState::Opening || to == TransportState::Idle || to == TransportState::Error || to == TransportState::Closed;
            
        case TransportState::Opening:
            return to == TransportState::Streaming || to == TransportState::Cancelled || to == TransportState::Error || to == TransportState::Ready;
            
        case TransportState::Streaming:
            return to == TransportState::Completed || to == TransportState::Cancelled || to == TransportState::Error || to == TransportState::Ready;
            
        case TransportState::Completed:
            return to == TransportState::Ready || to == TransportState::Error || to == TransportState::Closed;
            
        case TransportState::Cancelled:
            return to == TransportState::Ready || to == TransportState::Error || to == TransportState::Closed;
            
        case TransportState::Error:
            return to == TransportState::Ready || to == TransportState::Idle || to == TransportState::Closed;
            
        case TransportState::Closed:
            // Once closed, can only re-initialize from Idle
            return to == TransportState::Idle;
            
        default:
            return false;
    }
}

void HttpTransport::transitionTo(TransportState newState) {
    // First validate the transition is legal
    if (!isValidTransition(m_currentState, newState)) {
        #ifdef OTA_PLATFORM_VALIDATION
        // In validation mode: log but still proceed (fail-safe)
        LOG_WARN(LogTag::UPDATE, "Invalid transport state transition: %d -> %d", 
                 static_cast<uint8_t>(m_currentState), static_cast<uint8_t>(newState));
        #else
        // In production: stay in current state, set error, don't corrupt state machine
        m_lastError = TransportError::InternalError;
        transitionTo(TransportState::Error);
        return;
        #endif
    }
    
    // Only update state if transition is valid (or in validation mode we bypass this check)
    m_currentState = newState;
    m_progress.currentState = newState;
}

} // namespace Transport