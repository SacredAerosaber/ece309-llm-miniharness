# ECE 309 - Project 2 starter code

This is the repo for Project 2.

Everything under `include/model/`, `include/harness/`, `src/model_client.cpp`,
`src/scripted_client.cpp`, `src/replay_client.cpp`, `src/harness.cpp`, and
`src/main.cpp` is given code. This code was not modified by me.

The below files were written by me:

- `include/core/message.h` (+ optional `src/message.cpp`)
- `include/core/conversation.h` / `src/conversation.cpp`
- `include/core/sentinel_scanner.h` / `src/sentinel_scanner.cpp`
- `tests/p2/test_p2.cpp`
- `docs/design-log-p2.md`

## Build and run

```bash
cmake -S . -B build
cmake --build build
```

This builds two targets:

- `./build/miniharness` - the interactive CLI
- `./build/test_p2` - the test suite

Try it once `Conversation` and `SentinelScanner` both compile:

```bash
./build/miniharness --script scripts/greeting.script --save transcript.txt
```

Press Ctrl-D on an empty line to end the conversation early.
