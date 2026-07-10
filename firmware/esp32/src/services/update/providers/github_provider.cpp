#include "github_provider.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr const char* GITHUB_API_HOST = "https://api.github.com";
constexpr const char* GITHUB_MANIFEST_ASSET = "manifest.json";
constexpr const char* GITHUB_USER_AGENT = "DeskDroid-OTA";

bool isTokenPresent(const char* token) {
    return token != nullptr && token[0] != '\0';
}
} // namespace

const char* githubProviderStateToString(GitHubProviderState state) {
    switch (state) {
        case GitHubProviderState::Idle: return "Idle";
        case GitHubProviderState::Connecting: return "Connecting";
        case GitHubProviderState::FetchingRelease: return "FetchingRelease";
        case GitHubProviderState::FetchingManifest: return "FetchingManifest";
        case GitHubProviderState::ParsingManifest: return "ParsingManifest";
        case GitHubProviderState::Ready: return "Ready";
        case GitHubProviderState::Error: return "Error";
        default: return "Unknown";
    }
}

GitHubProvider::GitHubProvider()
    : _state(GitHubProviderState::Idle)
    , _previousState(GitHubProviderState::Idle)
    , _lastError(UpdateError::ok())
    , _begun(false)
    , _manifestParsed(false) {
    clearRelease();
    clearManifest();
    clearDiagnostics();
    clearBuffers();
    clearUpdateInfo();
}

GitHubProvider::GitHubProvider(const GitHubProviderConfig& config)
    : GitHubProvider() {
    configure(config);
}

bool GitHubProvider::configure(const GitHubProviderConfig& config) {
    if (_state != GitHubProviderState::Idle && _state != GitHubProviderState::Error) {
        return false;
    }

    _config = config;
    return true;
}

bool GitHubProvider::begin() {
    if (!isConfigured()) {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub provider configuration is incomplete");
        return false;
    }

    _begun = true;
    _lastError = UpdateError::ok();
    return true;
}

void GitHubProvider::loop() {
    // Provider is pull-driven by checkForUpdate().
}

bool GitHubProvider::available() const {
    return _state == GitHubProviderState::Ready && _latestUpdateInfo.isValid();
}

const char* GitHubProvider::providerName() const {
    return "GitHub Releases";
}

UpdateDecisionContext GitHubProvider::checkForUpdate() {
    if (_state == GitHubProviderState::Ready) {
        return UpdateDecisionContext::withUpdateInfo(
            UpdateDecisionType::UpdateAvailable,
            "GitHub release metadata is available",
            _latestUpdateInfo);
    }

    if (_state == GitHubProviderState::Error) {
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    clearRelease();
    clearManifest();
    clearBuffers();
    clearUpdateInfo();
    _manifestParsed = false;
    _diagnostics.lastUpdateCheckTimestamp = millis();

    if (!transitionTo(GitHubProviderState::Connecting)) {
        setError(UpdateErrorCode::StateInvalidTransition, "Failed to enter Connecting state");
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    if (!_begun && !begin()) {
        transitionTo(GitHubProviderState::Error);
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    if (!transitionTo(GitHubProviderState::FetchingRelease) || !fetchRelease()) {
        transitionTo(GitHubProviderState::Error);
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    if (!transitionTo(GitHubProviderState::FetchingManifest) || !fetchManifest()) {
        transitionTo(GitHubProviderState::Error);
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    if (!transitionTo(GitHubProviderState::ParsingManifest) || !parseManifest(_manifestBuffer)) {
        transitionTo(GitHubProviderState::Error);
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    if (!resolveFirmwareUrl(_releaseBuffer, _manifest.firmware) || !populateUpdateInfo()) {
        transitionTo(GitHubProviderState::Error);
        return UpdateDecisionContext::create(UpdateDecisionType::Error, _lastError.message());
    }

    transitionTo(GitHubProviderState::Ready);
    return UpdateDecisionContext::withUpdateInfo(
        UpdateDecisionType::UpdateAvailable,
        "GitHub release metadata is available",
        _latestUpdateInfo);
}

const UpdateInfo& GitHubProvider::latestUpdateInfo() const {
    return _latestUpdateInfo;
}

const UpdateError& GitHubProvider::lastError() const {
    return _lastError;
}

void GitHubProvider::reset() {
    clearRelease();
    clearManifest();
    clearDiagnostics();
    clearBuffers();
    clearUpdateInfo();
    _lastError = UpdateError::ok();
    _manifestParsed = false;
    _begun = false;
    _previousState = _state;
    _state = GitHubProviderState::Idle;
}

GitHubProviderState GitHubProvider::state() const {
    return _state;
}

const GitHubProviderDiagnostics& GitHubProvider::diagnostics() const {
    return _diagnostics;
}

bool GitHubProvider::transitionTo(GitHubProviderState newState) {
    if (!isLegalTransition(_state, newState)) {
#ifdef OTA_PLATFORM_VALIDATION
        printf("[GitHubProvider][INVARIANT] Illegal transition rejected: %s -> %s\n",
               githubProviderStateToString(_state),
               githubProviderStateToString(newState));
#endif
        return false;
    }

    if (_state == newState) {
        return true;
    }

    const GitHubProviderState previous = _state;
    _previousState = previous;
    _state = newState;

#ifdef OTA_PLATFORM_VALIDATION
    logTransition(previous, newState);
    validateInvariants(previous);
#endif

    return true;
}

bool GitHubProvider::isLegalTransition(GitHubProviderState from, GitHubProviderState to) const {
    if (from == to) {
        return true;
    }

    if (from == GitHubProviderState::Error) {
        return false;
    }

    if (from == GitHubProviderState::Idle && to == GitHubProviderState::Connecting) {
        return true;
    }

    if (from == GitHubProviderState::Connecting &&
        (to == GitHubProviderState::FetchingRelease || to == GitHubProviderState::Error)) {
        return true;
    }

    if (from == GitHubProviderState::FetchingRelease &&
        (to == GitHubProviderState::FetchingManifest || to == GitHubProviderState::Error)) {
        return true;
    }

    if (from == GitHubProviderState::FetchingManifest &&
        (to == GitHubProviderState::ParsingManifest || to == GitHubProviderState::Error)) {
        return true;
    }

    if (from == GitHubProviderState::ParsingManifest &&
        (to == GitHubProviderState::Ready || to == GitHubProviderState::Error)) {
        return true;
    }

    return false;
}

bool GitHubProvider::isConfigured() const {
    return _config.owner != nullptr && _config.owner[0] != '\0' &&
           _config.repository != nullptr && _config.repository[0] != '\0' &&
           _config.timeoutMs > 0;
}

void GitHubProvider::clearRelease() {
    memset(&_release, 0, sizeof(_release));
}

void GitHubProvider::clearManifest() {
    memset(&_manifest, 0, sizeof(_manifest));
}

void GitHubProvider::clearDiagnostics() {
    memset(&_diagnostics, 0, sizeof(_diagnostics));
    _diagnostics.lastHttpStatus = 0;
    _diagnostics.lastGitHubApiError = UpdateErrorCode::None;
    _diagnostics.lastJsonError = UpdateErrorCode::None;
}

void GitHubProvider::clearBuffers() {
    memset(_releaseBuffer, 0, sizeof(_releaseBuffer));
    memset(_manifestBuffer, 0, sizeof(_manifestBuffer));
}

void GitHubProvider::clearUpdateInfo() {
    _latestUpdateInfo = UpdateInfo();
}

void GitHubProvider::setError(UpdateErrorCode code, const char* message) {
    _lastError = UpdateError::create(code, message != nullptr ? message : "GitHub provider error");

    if (static_cast<int>(code) >= 100 && static_cast<int>(code) < 200) {
        _diagnostics.lastGitHubApiError = code;
    }

    if (static_cast<int>(code) >= 200 && static_cast<int>(code) < 400) {
        _diagnostics.lastJsonError = code;
    }
}

bool GitHubProvider::buildLatestReleaseUrl(char* out, size_t outSize) const {
    if (out == nullptr || outSize == 0 || !isConfigured()) {
        return false;
    }

    const int written = snprintf(out, outSize, "%s/repos/%s/%s/releases/latest",
                                 GITHUB_API_HOST,
                                 _config.owner,
                                 _config.repository);
    return written > 0 && static_cast<size_t>(written) < outSize;
}

bool GitHubProvider::fetchUrl(const char* url, char* out, size_t outSize) {
    if (url == nullptr || out == nullptr || outSize < 2 || !isHttpsUrl(url)) {
        setError(UpdateErrorCode::SecurityUnsecureConnection, "GitHub provider requires HTTPS URLs");
        return false;
    }

    out[0] = '\0';
    const uint32_t started = millis();
    uint8_t attempts = 0;
    const uint8_t maxAttempts = static_cast<uint8_t>(_config.retryCount + 1);

    while (attempts < maxAttempts) {
        attempts++;
        _diagnostics.retryCount = static_cast<uint8_t>(attempts - 1);

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.setTimeout(_config.timeoutMs);

        if (!http.begin(client, url)) {
            _diagnostics.lastHttpStatus = 0;
            setError(UpdateErrorCode::NetworkConnectionFailed, "Failed to begin GitHub HTTPS request");
            http.end();
            continue;
        }

        http.addHeader("User-Agent", GITHUB_USER_AGENT);
        http.addHeader("Accept", "application/vnd.github+json");
        if (isTokenPresent(_config.personalAccessToken)) {
            char authHeader[160] = {};
            snprintf(authHeader, sizeof(authHeader), "Bearer %s", _config.personalAccessToken);
            http.addHeader("Authorization", authHeader);
        }

        const int status = http.GET();
        _diagnostics.lastHttpStatus = status;

        if (status != HTTP_CODE_OK) {
            setError(status == HTTPC_ERROR_READ_TIMEOUT ? UpdateErrorCode::NetworkTimeout
                                                        : UpdateErrorCode::NetworkHttpError,
                     "GitHub HTTPS request failed");
            http.end();
            continue;
        }

        WiFiClient* stream = http.getStreamPtr();
        size_t used = 0;
        while (http.connected() && stream != nullptr && stream->available()) {
            const int value = stream->read();
            if (value < 0) {
                break;
            }
            if (used + 1 >= outSize) {
                http.end();
                setError(UpdateErrorCode::ValidationIncompleteData, "GitHub response exceeded fixed metadata buffer");
                _diagnostics.requestDurationMs = millis() - started;
                return false;
            }
            out[used++] = static_cast<char>(value);
        }
        out[used] = '\0';
        http.end();

        _diagnostics.requestDurationMs = millis() - started;
        _lastError = UpdateError::ok();
        return true;
    }

    _diagnostics.requestDurationMs = millis() - started;
    return false;
}

bool GitHubProvider::fetchRelease() {
    char url[URL_BUFFER_SIZE] = {};
    if (!buildLatestReleaseUrl(url, sizeof(url))) {
        setError(UpdateErrorCode::ValidationInvalidInput, "Failed to build GitHub latest release URL");
        return false;
    }

    if (!fetchUrl(url, _releaseBuffer, sizeof(_releaseBuffer))) {
        return false;
    }

    return parseRelease(_releaseBuffer);
}

bool GitHubProvider::fetchManifest() {
    if (!_release.hasManifestUrl) {
        setError(UpdateErrorCode::ValidationIncompleteData, "Release manifest asset URL is missing");
        return false;
    }

    return fetchUrl(_release.manifestUrl, _manifestBuffer, sizeof(_manifestBuffer));
}

bool GitHubProvider::parseRelease(const char* json) {
    if (json == nullptr || json[0] == '\0') {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub release JSON is empty");
        return false;
    }

    if (!findJsonString(json, "tag_name", _release.tag, sizeof(_release.tag))) {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub release tag is missing");
        return false;
    }

    if (!findJsonUint32(json, "id", &_release.releaseId)) {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub release id is missing");
        return false;
    }

    if (!findJsonString(json, "published_at", _release.publishedAt, sizeof(_release.publishedAt))) {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub release published time is missing");
        return false;
    }

    if (!findAssetUrlByName(json, GITHUB_MANIFEST_ASSET, _release.manifestUrl, sizeof(_release.manifestUrl))) {
        setError(UpdateErrorCode::ValidationIncompleteData, "GitHub manifest asset URL is missing");
        return false;
    }

    _release.hasRelease = true;
    _release.hasManifestUrl = true;
    return true;
}

bool GitHubProvider::parseManifest(const char* json) {
    if (json == nullptr || json[0] == '\0') {
        setError(UpdateErrorCode::ValidationIncompleteData, "Firmware manifest JSON is empty");
        return false;
    }

    uint32_t schema = 0;
    if (!findJsonUint32(json, "schema", &schema)) {
        setError(UpdateErrorCode::ValidationIncompleteData, "Firmware manifest schema is missing");
        return false;
    }

    if (schema != MANIFEST_SCHEMA_VERSION) {
        setError(UpdateErrorCode::ValidationInvalidFormat, "Firmware manifest schema is unsupported");
        return false;
    }

    _manifest.schema = static_cast<uint8_t>(schema);
    if (!findJsonString(json, "version", _manifest.version, sizeof(_manifest.version)) ||
        !findJsonUint32(json, "build", &_manifest.build) ||
        !findJsonString(json, "hardware", _manifest.hardware, sizeof(_manifest.hardware)) ||
        !findJsonUint32(json, "size", &_manifest.size) ||
        !findJsonString(json, "sha256", _manifest.sha256, sizeof(_manifest.sha256)) ||
        !findJsonString(json, "firmware", _manifest.firmware, sizeof(_manifest.firmware))) {
        setError(UpdateErrorCode::ValidationIncompleteData, "Firmware manifest is missing required fields");
        return false;
    }

    if (strlen(_manifest.sha256) != 64 || _manifest.size == 0) {
        setError(UpdateErrorCode::ValidationInvalidFormat, "Firmware manifest contains invalid checksum or size");
        return false;
    }

    _manifest.parsed = true;
    _manifestParsed = true;
    copyString(_diagnostics.lastManifestVersion,
               sizeof(_diagnostics.lastManifestVersion),
               _manifest.version);
    return true;
}

bool GitHubProvider::resolveFirmwareUrl(const char* json, const char* firmwareAssetName) {
    if (firmwareAssetName == nullptr || firmwareAssetName[0] == '\0') {
        setError(UpdateErrorCode::ValidationIncompleteData, "Firmware asset name is missing");
        return false;
    }

    if (!findAssetUrlByName(json, firmwareAssetName, _release.firmwareUrl, sizeof(_release.firmwareUrl))) {
        setError(UpdateErrorCode::ValidationIncompleteData, "Firmware asset URL is missing from release");
        return false;
    }

    _release.hasFirmwareUrl = true;
    copyString(_release.firmwareAssetName, sizeof(_release.firmwareAssetName), firmwareAssetName);
    copyString(_diagnostics.lastFirmwareUrl, sizeof(_diagnostics.lastFirmwareUrl), _release.firmwareUrl);
    return true;
}

bool GitHubProvider::populateUpdateInfo() {
    if (!_manifest.parsed || !_release.hasFirmwareUrl) {
        setError(UpdateErrorCode::ValidationIncompleteData, "Cannot populate UpdateInfo before manifest and release are parsed");
        return false;
    }

    _latestUpdateInfo.version = _manifest.version;
    _latestUpdateInfo.numericVersion = _manifest.build;
    _latestUpdateInfo.downloadUrl = _release.firmwareUrl;
    _latestUpdateInfo.fileSize = _manifest.size;
    _latestUpdateInfo.sha256 = _manifest.sha256;
    _latestUpdateInfo.releaseNotes = _release.tag;
    _latestUpdateInfo.releaseDate = _release.publishedAt;
    _latestUpdateInfo.minCompatibleVersion = "";
    _latestUpdateInfo.hardwareVariant = _manifest.hardware;

    if (!_latestUpdateInfo.isValid()) {
        setError(UpdateErrorCode::ValidationInvalidFormat, "Parsed GitHub metadata did not produce valid UpdateInfo");
        clearUpdateInfo();
        return false;
    }

    _lastError = UpdateError::ok();
    return true;
}

bool GitHubProvider::findJsonString(const char* json, const char* key, char* out, size_t outSize) const {
    if (json == nullptr || key == nullptr || out == nullptr || outSize == 0) {
        return false;
    }

    char pattern[48] = {};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* found = strstr(json, pattern);
    if (found == nullptr) {
        return false;
    }

    const char* colon = strchr(found + strlen(pattern), ':');
    if (colon == nullptr) {
        return false;
    }

    const char* value = colon + 1;
    while (*value != '\0' && isspace(static_cast<unsigned char>(*value))) {
        value++;
    }
    if (*value != '"') {
        return false;
    }
    value++;

    size_t used = 0;
    while (*value != '\0' && *value != '"') {
        if (*value == '\\') {
            return false;
        }
        if (used + 1 >= outSize) {
            return false;
        }
        out[used++] = *value++;
    }

    if (*value != '"') {
        return false;
    }

    out[used] = '\0';
    return used > 0;
}

bool GitHubProvider::findJsonUint32(const char* json, const char* key, uint32_t* out) const {
    if (json == nullptr || key == nullptr || out == nullptr) {
        return false;
    }

    char pattern[48] = {};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* found = strstr(json, pattern);
    if (found == nullptr) {
        return false;
    }

    const char* colon = strchr(found + strlen(pattern), ':');
    if (colon == nullptr) {
        return false;
    }

    const char* value = colon + 1;
    while (*value != '\0' && isspace(static_cast<unsigned char>(*value))) {
        value++;
    }

    if (!isdigit(static_cast<unsigned char>(*value))) {
        return false;
    }

    uint32_t parsed = 0;
    while (isdigit(static_cast<unsigned char>(*value))) {
        parsed = (parsed * 10U) + static_cast<uint32_t>(*value - '0');
        value++;
    }

    *out = parsed;
    return true;
}

bool GitHubProvider::findAssetUrlByName(const char* json, const char* assetName, char* out, size_t outSize) const {
    if (json == nullptr || assetName == nullptr || out == nullptr || outSize == 0) {
        return false;
    }

    const char* cursor = json;
    while ((cursor = strstr(cursor, "\"name\"")) != nullptr) {
        char name[64] = {};
        if (!findJsonString(cursor, "name", name, sizeof(name))) {
            cursor += 6;
            continue;
        }

        if (strcmp(name, assetName) == 0) {
            return findJsonString(cursor, "browser_download_url", out, outSize) && isHttpsUrl(out);
        }

        cursor += 6;
    }

    return false;
}

bool GitHubProvider::isHttpsUrl(const char* url) const {
    return url != nullptr && strncmp(url, "https://", 8) == 0;
}

void GitHubProvider::copyString(char* dest, size_t destSize, const char* source) const {
    if (dest == nullptr || destSize == 0) {
        return;
    }

    dest[0] = '\0';
    if (source == nullptr) {
        return;
    }

    strncpy(dest, source, destSize - 1);
    dest[destSize - 1] = '\0';
}

#ifdef OTA_PLATFORM_VALIDATION
void GitHubProvider::logTransition(GitHubProviderState from, GitHubProviderState to) const {
    printf("[GitHubProvider] State transition: %s -> %s\n",
           githubProviderStateToString(from),
           githubProviderStateToString(to));
}

void GitHubProvider::validateInvariants(GitHubProviderState previousState) const {
    if (_state == GitHubProviderState::Ready && !_manifestParsed) {
        printf("[GITHUB_PROVIDER][INVARIANT] Ready requires manifest parsed successfully\n");
    }

    if (_state == GitHubProviderState::Ready && !_release.hasFirmwareUrl) {
        printf("[GITHUB_PROVIDER][INVARIANT] Ready requires firmware URL valid\n");
    }

    if (_state == GitHubProviderState::Ready && _latestUpdateInfo.numericVersion == 0) {
        printf("[GITHUB_PROVIDER][INVARIANT] Ready requires version parsed\n");
    }

    if (_state == GitHubProviderState::Ready && _latestUpdateInfo.sha256.length() != 64) {
        printf("[GITHUB_PROVIDER][INVARIANT] Ready requires checksum present\n");
    }

    if (_state == GitHubProviderState::Ready && _latestUpdateInfo.fileSize == 0) {
        printf("[GITHUB_PROVIDER][INVARIANT] Ready requires firmware size > 0\n");
    }

    if (_state == GitHubProviderState::Error && _latestUpdateInfo.isValid()) {
        printf("[GITHUB_PROVIDER][INVARIANT] Error requires UpdateInfo not modified\n");
    }

    if (_state == GitHubProviderState::ParsingManifest && !_release.hasRelease) {
        printf("[GITHUB_PROVIDER][INVARIANT] ParsingManifest requires release metadata available\n");
    }

    if (_state == GitHubProviderState::FetchingManifest && !_release.hasManifestUrl) {
        printf("[GITHUB_PROVIDER][INVARIANT] FetchingManifest requires manifest URL exists\n");
    }

    if (_state == GitHubProviderState::FetchingRelease && previousState != GitHubProviderState::Connecting) {
        printf("[GITHUB_PROVIDER][INVARIANT] FetchingRelease requires HTTP session active\n");
    }
}
#endif
