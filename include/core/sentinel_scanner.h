// sentinel_scanner.h
// Looks for a specific stop string (the "sentinel") inside text that
// arrives as a bunch of separate chunks. The tricky part is that the
// sentinel can be split across two or more chunks at any point, so we
// can't just check each chunk on its own, we have to remember a little
// bit of state between calls.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel) : sentinel_(std::move(sentinel)) {}

    struct Out {
        std::string safe_text;
        bool sentinel_found;
    };

    // Feed it the next chunk of text. Returns whatever text is safe to
    // print right away (definitely not part of the sentinel), and
    // whether the full sentinel was just found.
    Out feed(std::string_view chunk);

    // Call once after the stream is finished to get back any leftover
    // text that was being held onto in case it turned into a sentinel.
    Out flush();

    // Not part of what the harness actually needs, just added this so
    // my tests can check pending_ never grows past its bound.
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;  // holds at most sentinel_.size() - 1 chars
};
