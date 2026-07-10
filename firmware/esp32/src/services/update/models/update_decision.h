#pragma once

#include <Arduino.h>
#include <string>
#include "update_info.h"

/**
 * @brief Update decision enumeration for available update assessment
 * 
 * Defines all possible outcomes when evaluating whether an available
 * firmware update should be installed. Provides nuanced classification
 * from no update available to mandatory updates.
 */
enum class UpdateDecisionType {
    NoUpdateAvailable,    ///< No newer firmware version available
    UpdateAvailable,      ///< Optional update is available for installation
    UpdateRecommended,    ///< Update is recommended (contains bug fixes/improvements)
    UpdateRequired,       ///< Update is mandatory (critical security/stability fixes)
    Deferred,             ///< User chose to defer this update
    Skipped,              ///< User chose to skip this specific update version
    Error                 ///< An error occurred during update evaluation
};

/**
 * @brief Converts an UpdateDecisionType to a human-readable string
 * @param type The decision type to convert
 * @return Const char* with string representation
 */
inline const char* updateDecisionTypeToString(UpdateDecisionType type) {
    switch (type) {
        case UpdateDecisionType::NoUpdateAvailable: return "NoUpdateAvailable";
        case UpdateDecisionType::UpdateAvailable:    return "UpdateAvailable";
        case UpdateDecisionType::UpdateRecommended:  return "UpdateRecommended";
        case UpdateDecisionType::UpdateRequired:     return "UpdateRequired";
        case UpdateDecisionType::Deferred:           return "Deferred";
        case UpdateDecisionType::Skipped:            return "Skipped";
        case UpdateDecisionType::Error:              return "Error";
        default:                                     return "Unknown";
    }
}

/**
 * @brief Simple wrapper for an update decision result
 * 
 * Lightweight wrapper around the decision type that represents the
 * primary outcome of an update check. Separates the core decision
 * from the full context details for clean API separation.
 */
struct UpdateDecision {
    /// The type of decision made
    UpdateDecisionType type;
    
    /**
     * @brief Default constructor initializes to NoUpdateAvailable
     */
    UpdateDecision() : type(UpdateDecisionType::NoUpdateAvailable) {}
    
    /**
     * @brief Construct from an UpdateDecisionType
     * @param t The decision type
     */
    UpdateDecision(UpdateDecisionType t) : type(t) {}
    
    /**
     * @brief Get the human-readable string representation
     * @return Const char* with string representation
     */
    const char* toString() const {
        return updateDecisionTypeToString(type);
    }
};

/**
 * @brief Complete context for an update decision
 * 
 * Combines the decision type with a human-readable reason and optional
 * reference to the UpdateInfo that the decision was based on. Provides
 * complete context for logging, UI display, and user notification.
 */
struct UpdateDecisionContext {
    /// The type of decision made
    UpdateDecisionType decision;
    
    /// Human-readable explanation for the decision
    std::string reason;
    
    /// Flag indicating if updateInfo is valid/populated
    bool hasUpdateInfo;
    
    /// The evaluated update information (only valid if hasUpdateInfo is true)
    UpdateInfo updateInfo;
    
    /**
     * @brief Create a context with just a decision and reason
     * @param d The decision type
     * @param r Explanation of the decision
     * @return UpdateDecisionContext instance
     */
    static UpdateDecisionContext create(UpdateDecisionType d, const std::string& r) {
        UpdateDecisionContext ctx;
        ctx.decision = d;
        ctx.reason = r;
        ctx.hasUpdateInfo = false;
        return ctx;
    }
    
    /**
     * @brief Create a context with decision, reason, and update info
     * @param d The decision type
     * @param r Explanation of the decision
     * @param info The evaluated UpdateInfo
     * @return UpdateDecisionContext instance
     */
    static UpdateDecisionContext withUpdateInfo(UpdateDecisionType d, const std::string& r, const UpdateInfo& info) {
        UpdateDecisionContext ctx;
        ctx.decision = d;
        ctx.reason = r;
        ctx.hasUpdateInfo = true;
        ctx.updateInfo = info;
        return ctx;
    }
};