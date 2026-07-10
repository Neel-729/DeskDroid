#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "update_provider.h"

enum class GitHubProviderState : uint8_t {
    Idle,
    Connecting,
    FetchingRelease,
    FetchingManifest,
    ParsingManifest,
    Ready,
    Error
};

struct GitHubProviderConfig {
    const char* owner;
    const char* repository;
    const char* personalAccessToken;
    uint32_t timeoutMs;
    uint8_t retryCount;

    GitHubProviderConfig()
        : owner(nullptr)
        , repository(nullptr)
        , personalAccessToken(nullptr)
        , timeoutMs(10000)
        , retryCount(1) {}
};

struct GitHubProviderDiagnostics {
    int lastHttpStatus;
    UpdateErrorCode lastGitHubApiError;
    UpdateErrorCode lastJsonError;
    char lastManifestVersion[24];
    char lastFirmwareUrl[192];
    uint32_t lastUpdateCheckTimestamp;
    uint8_t retryCount;
    uint32_t requestDurationMs;
};

class GitHubProvider : public IUpdateProvider {
public:
    GitHubProvider();
    explicit GitHubProvider(const GitHubProviderConfig& config);

    bool configure(const GitHubProviderConfig& config);
    bool begin() override;
    void loop() override;
    bool available() const override;
    const char* providerName() const override;
    UpdateDecisionContext checkForUpdate() override;
    const UpdateInfo& latestUpdateInfo() const override;
    const UpdateError& lastError() const override;
    void reset() override;

    GitHubProviderState state() const;
    const GitHubProviderDiagnostics& diagnostics() const;

private:
    struct ReleaseMetadata {
        char tag[32];
        uint32_t releaseId;
        char publishedAt[32];
        char manifestUrl[192];
        char firmwareUrl[192];
        char firmwareAssetName[64];
        bool hasRelease;
        bool hasManifestUrl;
        bool hasFirmwareUrl;
    };

    struct ManifestMetadata {
        uint8_t schema;
        char version[24];
        uint32_t build;
        char hardware[32];
        uint32_t size;
        char sha256[65];
        char firmware[64];
        bool parsed;
    };

    static constexpr size_t RELEASE_BUFFER_SIZE = 4096;
    static constexpr size_t MANIFEST_BUFFER_SIZE = 1024;
    static constexpr size_t URL_BUFFER_SIZE = 192;
    static constexpr uint8_t MANIFEST_SCHEMA_VERSION = 1;

    bool transitionTo(GitHubProviderState newState);
    bool isLegalTransition(GitHubProviderState from, GitHubProviderState to) const;
    bool isConfigured() const;
    void clearRelease();
    void clearManifest();
    void clearDiagnostics();
    void clearBuffers();
    void clearUpdateInfo();
    void setError(UpdateErrorCode code, const char* message);
    bool buildLatestReleaseUrl(char* out, size_t outSize) const;
    bool fetchUrl(const char* url, char* out, size_t outSize);
    bool fetchRelease();
    bool fetchManifest();
    bool parseRelease(const char* json);
    bool parseManifest(const char* json);
    bool resolveFirmwareUrl(const char* json, const char* firmwareAssetName);
    bool populateUpdateInfo();
    bool findJsonString(const char* json, const char* key, char* out, size_t outSize) const;
    bool findJsonUint32(const char* json, const char* key, uint32_t* out) const;
    bool findAssetUrlByName(const char* json, const char* assetName, char* out, size_t outSize) const;
    bool isHttpsUrl(const char* url) const;
    void copyString(char* dest, size_t destSize, const char* source) const;

#ifdef OTA_PLATFORM_VALIDATION
    void logTransition(GitHubProviderState from, GitHubProviderState to) const;
    void validateInvariants(GitHubProviderState previousState) const;
#endif

    GitHubProviderConfig _config;
    GitHubProviderState _state;
    GitHubProviderState _previousState;
    GitHubProviderDiagnostics _diagnostics;
    ReleaseMetadata _release;
    ManifestMetadata _manifest;
    UpdateInfo _latestUpdateInfo;
    UpdateError _lastError;
    char _releaseBuffer[RELEASE_BUFFER_SIZE];
    char _manifestBuffer[MANIFEST_BUFFER_SIZE];
    bool _begun;
    bool _manifestParsed;
};

const char* githubProviderStateToString(GitHubProviderState state);
