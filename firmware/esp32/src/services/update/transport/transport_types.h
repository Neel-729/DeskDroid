#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace Transport {

/**
 * @brief Transport state machine states
 */
enum class TransportState : uint8_t {
    Idle,           ///< Transport is idle and ready
    Initializing,   ///< Transport is initializing
    Ready,          ///< Transport is ready to accept requests
    Opening,        ///< Transport is opening a connection/stream
    Streaming,      ///< Transport is actively transferring data
    Completed,      ///< Transfer completed successfully
    Cancelled,      ///< Transfer was cancelled
    Error,          ///< An error occurred during transfer
    Closed          ///< Transport has been closed/shutdown
};

/**
 * @brief Transport error codes - strongly typed errors
 */
enum class TransportError : uint8_t {
    None,           ///< No error
    NotInitialized, ///< Transport not initialized
    Busy,           ///< Transport is busy with another transfer
    Cancelled,      ///< Operation was cancelled
    ConnectionFailed, ///< Could not establish connection
    Timeout,        ///< Operation timed out
    Unsupported,    ///< Operation not supported by this transport
    InvalidRequest, ///< Invalid request parameters
    InternalError,  ///< Internal transport error
    OutOfMemory,    ///< Memory allocation failed
    Unknown         ///< Unknown error
};

/**
 * @brief Transport capability flags
 */
struct TransportCapabilities {
    bool supportsResume : 1;        ///< Supports resuming interrupted transfers
    bool supportsStreaming : 1;     ///< Supports streaming data
    bool supportsSeek : 1;          ///< Supports seeking to specific offsets
    bool supportsParallel : 1;      ///< Supports multiple concurrent transfers
    bool hasSecureTransport : 1;    ///< Transport is secure (HTTPS, TLS, etc.)
    
    TransportCapabilities() 
        : supportsResume(false)
        , supportsStreaming(false)
        , supportsSeek(false)
        , supportsParallel(false)
        , hasSecureTransport(false) {}
};

/**
 * @brief Transport progress tracking - reusable progress reporting
 */
struct TransportProgress {
    uint32_t bytesTransferred;      ///< Total bytes transferred so far
    uint32_t expectedSize;          ///< Total expected size in bytes (0 if unknown)
    uint32_t transferSpeed;         ///< Current transfer speed in bytes/sec (placeholder)
    uint32_t estimatedRemaining;    ///< Estimated remaining bytes to transfer (placeholder)
    TransportState currentState;    ///< Current state of the transfer
    
    TransportProgress()
        : bytesTransferred(0)
        , expectedSize(0)
        , transferSpeed(0)
        , estimatedRemaining(0)
        , currentState(TransportState::Idle) {}
    
    /**
     * @brief Calculate completion percentage
     * @return Percentage complete (0-100), 0 if size is unknown
     */
    uint8_t percentage() const {
        if (expectedSize == 0) return 0;
        return static_cast<uint8_t>((static_cast<uint64_t>(bytesTransferred) * 100) / expectedSize);
    }
    
    /**
     * @brief Reset progress to initial state
     */
    void reset() {
        bytesTransferred = 0;
        expectedSize = 0;
        transferSpeed = 0;
        estimatedRemaining = 0;
        currentState = TransportState::Idle;
    }
};

/**
 * @brief Transport request - contains all information to start a transfer
 */
struct TransportRequest {
    const char* uri;                 ///< URI/URL/path to transfer from
    uint32_t offset;                 ///< Start offset (for resume/seeking)
    size_t bufferSize;               ///< Preferred buffer size
    bool streamRequested;            ///< Request streaming mode
    
    TransportRequest()
        : uri(nullptr)
        , offset(0)
        , bufferSize(1024)
        , streamRequested(false) {}
    
    /**
     * @brief Create a simple request with just a URI
     * @param uri The URI to transfer from
     * @return TransportRequest instance
     */
    static TransportRequest simple(const char* uri) {
        TransportRequest req;
        req.uri = uri;
        return req;
    }
};

/**
 * @brief Transport response - contains response information after opening a transfer
 */
struct TransportResponse {
    bool success;                    ///< Whether the request was accepted
    TransportError error;            ///< Error code if failed
    uint32_t contentLength;          ///< Content length if available
    const char* contentType;         ///< MIME type if available
    int32_t httpStatusCode;          ///< HTTP status code (only for HTTP transports)
    
    TransportResponse()
        : success(false)
        , error(TransportError::None)
        , contentLength(0)
        , contentType(nullptr)
        , httpStatusCode(0) {}
};

/**
 * @brief Transport result - final result of a complete transfer
 */
struct TransportResult {
    bool completed;                  ///< Whether transfer completed successfully
    TransportError error;            ///< Final error state
    uint32_t totalBytesTransferred;  ///< Total bytes transferred
    uint32_t totalDuration;          ///< Total transfer duration in ms
    
    TransportResult()
        : completed(false)
        , error(TransportError::None)
        , totalBytesTransferred(0)
        , totalDuration(0) {}
};

} // namespace Transport