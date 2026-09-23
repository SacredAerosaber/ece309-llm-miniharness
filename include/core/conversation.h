// conversation.h
// Growable array of Message objects. Basically a mini version of
// std::vector, but we're not allowed to use vector for this class, so
// this manages its own memory with new/delete.

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Starts empty, doesn't allocate anything until the first append.
    Conversation() noexcept = default;

    // Frees the array we own.
    ~Conversation();

    // Copying makes a totally separate array (deep copy), not just
    // another pointer to the same memory.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Moving just takes the other object's pointer instead of copying
    // every message over, and leaves the other object empty.
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Adds a message to the end. Grows the array first if it's full.
    void append(Message m);

    std::size_t size() const noexcept { return size_; }

    // Throws std::out_of_range if i is too big, instead of reading
    // memory we don't own.
    const Message& at(std::size_t i) const;

    // So we can use range-based for loops on a Conversation.
    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

private:
    // Grows the backing array to hold at least new_capacity messages.
    void reserve(std::size_t new_capacity);

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
