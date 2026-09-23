# Design Log - Project 2

## Conversation: growth factor

I used a doubling growth factor for the array inside `Conversation`. It
starts empty (capacity 0), and the first `append()` grows it to
capacity 1. After that, every time `size_` hits `capacity_`, I double
the capacity instead of growing it by a fixed amount like 1 or 10.

The reason is how often `append()` has to actually copy or move data.
If capacity only grew by 1 each time, every single append would need a
full reallocation that touches every existing element. That's roughly
N + (N-1) + (N-2) + ... which is O(N^2) total work for N appends.

Doubling avoids this. Reallocations only happen at sizes 1, 2, 4, 8,
16, and so on, so N appends only trigger about log2(N) reallocations.
Each one moves at most twice as many elements as the last, so summed
across all of them the total work is less than 2N. Spread over N
appends, that's O(1) per append on average, even though the specific
appends that trigger a reallocation cost more than the rest. That's
what "amortized O(1)" means: not every append is equally fast, but the
average cost per append stays constant as N grows.

## Rule of Five

`Conversation` owns raw heap memory (`Message* data_`), so it needs all
five special member functions or it will either leak memory or
double-free it:

- Destructor: frees `data_` with `delete[]`.
- Copy constructor / copy assignment: allocate a new buffer and
  copy every `Message` over, so two `Conversation` objects never share
  the same underlying array. Without this, the compiler's default
  copy would just copy the pointer, and both objects would end up
  pointing at the same memory, then both would try to free it when
  they're destroyed.
- Move constructor / move assignment: instead of copying, just
  steal the other object's pointer and null it out. This avoids
  copying every message when we don't need to, like when a
  `Conversation` gets returned from a function.

I tested this directly in `test_p2.cpp` by checking that a copy's
`begin()` pointer is different from the original's (proves it's a real
deep copy, not just copying the pointer), and that a move's `begin()`
pointer is the same as the original had before moving, while the
moved-from object goes back to size 0.

## SentinelScanner: bounded memory

`SentinelScanner` only ever holds back at most `sentinel.size() - 1`
characters in `pending_`. Every call to `feed()` appends the new chunk,
searches for the sentinel, and if it's not found, releases everything
except the last `sentinel.size() - 1` characters. Those last few
characters are the only ones that could still turn into the start of a
sentinel once more text arrives, anything before that is guaranteed to
not be part of an unfinished sentinel match, so it's safe to discard. 
I stress tested this in `ScannerBoundedMemory` by feeding a
scanner one byte at a time from a stream that constantly almost
matches the sentinel but never completes it, and checking
`pending_size()` never goes over the limit.

## What I would do differently

If I redid this project, I would guard `capacity_ * 2` against overflow. 
It doesn't matter at the scale of this project, but 
it is a better habit for a growable array meant to
handle arbitrary input sizes.
