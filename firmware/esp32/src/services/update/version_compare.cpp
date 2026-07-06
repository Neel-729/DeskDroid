#include "version_compare.h"

namespace VersionCompare {
VersionComparison compare(uint32_t currentVersionCode, uint32_t remoteVersionCode) {
    if (remoteVersionCode > currentVersionCode) {
        return VersionComparison::Newer;
    } else if (remoteVersionCode < currentVersionCode) {
        return VersionComparison::Older;
    } else {
        return VersionComparison::Equal;
    }
}
}