// sentinel_scanner.cpp
//
// How this works: every time feed() runs, we stick the new chunk onto
// whatever we were already holding in pending_, then search that whole
// buffer for the sentinel.
//
// If we find it, everything before it is safe text, and we can drop the
// sentinel itself (and anything after it, the harness stops caring once
// it sees the sentinel).
//
// If we don't find it, we can safely hand back everything except the
// last (sentinel_.size() - 1) characters, since those last few could
// still be the start of a sentinel that isn't finished yet. That's what
// keeps pending_ from growing without bound, no matter how much text
// comes through.

#include "core/sentinel_scanner.h"

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    pending_.append(chunk.data(), chunk.size());

    Out result;
    result.sentinel_found = false;

    std::size_t pos = pending_.find(sentinel_);
    if (pos != std::string::npos) {
        result.safe_text = pending_.substr(0, pos);
        result.sentinel_found = true;
        pending_.clear();
        return result;
    }

    // No match yet, so keep back the last (sentinel_.size() - 1)
    // characters and release everything before that.
    std::size_t keep = sentinel_.size() - 1;
    if (pending_.size() > keep) {
        std::size_t safe_len = pending_.size() - keep;
        result.safe_text = pending_.substr(0, safe_len);
        pending_.erase(0, safe_len);
    }
    // else: pending_ is still short enough that all of it could be the
    // start of a sentinel, so we don't release anything this time.

    return result;
}

SentinelScanner::Out SentinelScanner::flush() {
    Out result;
    result.safe_text = pending_;
    result.sentinel_found = false;
    pending_.clear();
    return result;
}
