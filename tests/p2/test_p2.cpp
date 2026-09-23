// tests/p2/test_p2.cpp
//
// Test suite for P2. 12 tests total, mostly focused on Conversation
// and SentinelScanner since that's the code I wrote. The
// Harness tests near the bottom just check that my classes plug into
// the provided loop correctly.

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

// Small fake InputSource for feeding fixed lines into Harness::run()
// like it was reading from stdin. It's fine to use std::vector/string
// here, the "no vector" rule is only about Conversation's storage.
class FakeInput : public InputSource {
public:
    explicit FakeInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (idx_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[idx_++];
    }
    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t idx_ = 0;
    bool eof_ = false;
};

// Throws away everything written to it, we only care about the
// StopReason in the harness tests below.
class NullOutput : public OutputSink {
public:
    void write(std::string_view) override {}
};

const char* role_name(Role role) {
    switch (role) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

// Same transcript format main.cpp uses, just copied here so the
// round-trip test doesn't depend on main.cpp's private helper.
void write_transcript(const std::string& path, const Conversation& conv) {
    std::ofstream file(path);
    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(m->role()) << "\n" << m->content() << "\n";
    }
}

int tests_passed = 0;

void run_test(const char* name, void (*test_fn)()) {
    test_fn();
    ++tests_passed;
    std::cout << "PASS: " << name << "\n";
}

// test 1: an empty Conversation shouldn't let you read out of bounds
void test_empty_conversation_bounds() {
    Conversation c;
    assert(c.size() == 0);
    assert(c.begin() == c.end());

    bool threw = false;
    try {
        c.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

// test 2: system message has to stay first, even after more turns
// get added on
void test_system_message_stays_first() {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    c.append(Message(Role::User, "hi"));
    c.append(Message(Role::Assistant, "hello"));

    assert(c.size() == 3);
    assert(c.at(0).role() == Role::System);
    assert(c.at(0).content() == "Be concise.");

    for (int i = 0; i < 10; ++i) {
        c.append(Message(Role::User, "more"));
    }
    assert(c.at(0).role() == Role::System);
}

// test 3: copy constructor / copy assignment should be a real deep copy
void test_copy_is_deep() {
    Conversation a;
    a.append(Message(Role::User, "one"));
    a.append(Message(Role::Assistant, "two"));

    Conversation b(a);
    assert(b.begin() != a.begin());  // different memory, not just aliased
    assert(b.size() == a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a.at(i).content() == b.at(i).content());
    }

    // changing b shouldn't touch a at all
    b.append(Message(Role::User, "three"));
    assert(a.size() == 2);
    assert(b.size() == 3);

    // now check operator= too, not just the constructor
    Conversation d;
    d.append(Message(Role::User, "placeholder"));
    d = a;
    assert(d.begin() != a.begin());
    assert(d.size() == a.size());
    assert(d.at(0).content() == "one");
}

// test 4: move constructor / move assignment should steal the buffer,
// not copy it, and leave the source empty
void test_move_steals_pointer() {
    Conversation e;
    e.append(Message(Role::User, "x"));
    const Message* original_ptr = e.begin();

    Conversation f(std::move(e));
    assert(f.begin() == original_ptr);
    assert(f.size() == 1);
    assert(e.size() == 0);
    assert(e.begin() == e.end());

    Conversation g;
    g.append(Message(Role::User, "y"));
    Conversation h;
    h.append(Message(Role::User, "placeholder"));
    h = std::move(g);
    assert(h.size() == 1);
    assert(h.at(0).content() == "y");
    assert(g.size() == 0);
}

// test 5: append a bunch of messages and make sure size()/at() stay
// correct the whole way through, even as the array grows multiple times.
// (capacity_ is private so I can't check the actual growth factor from
// out here, this just checks the end result is still correct)
void test_growth_keeps_data_correct() {
    Conversation c;
    const int count = 500;
    for (int i = 0; i < count; ++i) {
        c.append(Message(Role::User, "msg" + std::to_string(i)));
    }
    assert(c.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        assert(c.at(static_cast<std::size_t>(i)).content() == "msg" + std::to_string(i));
    }
}

// test 6: plain text with no sentinel in it should just pass through
void test_scanner_no_sentinel() {
    SentinelScanner scanner("<|end_conversation|>");
    auto out = scanner.feed("Hello, how can I help you today?");
    auto flushed = scanner.flush();
    assert(out.safe_text + flushed.safe_text == "Hello, how can I help you today?");
    assert(!out.sentinel_found);
    assert(!flushed.sentinel_found);
}

// test 7: the sentinel has to be caught no matter where it gets split
// across two chunks. loops over every possible split point.
void test_scanner_catches_split_sentinel() {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert(out1.sentinel_found || out2.sentinel_found);
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

// test 8: text that just looks similar to the sentinel shouldn't
// trigger a false match
void test_scanner_no_false_positive() {
    SentinelScanner scanner("<|end_conversation|>");
    const std::string text = "<|end_world|> is not the sentinel";
    auto out = scanner.feed(text);
    assert(!out.sentinel_found);
    auto flushed = scanner.flush();
    assert(!flushed.sentinel_found);
    assert(out.safe_text + flushed.safe_text == text);
}

// test 9: pending_ should never grow past sentinel.size() - 1, even
// when we feed it an adversarial stream one byte at a time
void test_scanner_pending_stays_bounded() {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);
    const std::size_t max_allowed = sentinel.size() - 1;

    // repeats a string that's one character short of the real sentinel,
    // over and over, so it never actually completes
    const std::string almost_sentinel = "<|end_conversatio";
    std::string stream;
    for (int i = 0; i < 2000; ++i) stream += almost_sentinel;

    for (char ch : stream) {
        scanner.feed(std::string_view(&ch, 1));
        assert(scanner.pending_size() <= max_allowed);
    }
}

// test 10: max_turns should cut the loop off with TurnLimit if the
// model never says the sentinel
void test_harness_hits_turn_limit() {
    HarnessConfig cfg;
    cfg.max_turns = 1;

    auto model = std::make_unique<ScriptedModelClient>("scripts/greeting.script");
    Harness harness(std::move(model), cfg);

    FakeInput in({"hello"});
    NullOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::TurnLimit);
}

// test 11: the loop should stop as soon as the sentinel shows up.
// greeting.script has 3 replies and only the last one has the sentinel,
// so this needs 3 user turns to get there.
void test_harness_stops_on_sentinel() {
    HarnessConfig cfg;
    cfg.max_turns = 20;

    auto model = std::make_unique<ScriptedModelClient>("scripts/greeting.script");
    Harness harness(std::move(model), cfg);

    FakeInput in({"hi", "hi again", "bye"});
    NullOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::Sentinel);
}

// test 12: save a conversation to a transcript file, load it back with
// ReplayModelClient, make sure it plays back the same thing
void test_transcript_round_trip() {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi there!"));
    conv.append(Message(Role::User, "bye"));
    conv.append(Message(Role::Assistant, "Goodbye.<|end_conversation|>"));

    const std::string path = "test_p2_roundtrip_transcript.txt";
    write_transcript(path, conv);

    ReplayModelClient replay(path);
    assert(replay.system_message() == "Be concise.");

    Conversation unused;  // ReplayModelClient doesn't actually look at this
    Message reply1 = replay.generate(unused);
    assert(reply1.content() == "Hi there!");

    Message reply2 = replay.generate(unused);
    assert(reply2.content() == "Goodbye.<|end_conversation|>");

    std::remove(path.c_str());
}

}  // namespace

int main() {
    run_test("empty conversation bounds", test_empty_conversation_bounds);
    run_test("system message stays first", test_system_message_stays_first);
    run_test("copy is deep", test_copy_is_deep);
    run_test("move steals pointer", test_move_steals_pointer);
    run_test("growth keeps data correct", test_growth_keeps_data_correct);
    run_test("scanner: no sentinel", test_scanner_no_sentinel);
    run_test("scanner: catches split sentinel", test_scanner_catches_split_sentinel);
    run_test("scanner: no false positive", test_scanner_no_false_positive);
    run_test("scanner: pending stays bounded", test_scanner_pending_stays_bounded);
    run_test("harness: hits turn limit", test_harness_hits_turn_limit);
    run_test("harness: stops on sentinel", test_harness_stops_on_sentinel);
    run_test("transcript round trip", test_transcript_round_trip);

    std::cout << tests_passed << " tests passed\n";
    return 0;
}
