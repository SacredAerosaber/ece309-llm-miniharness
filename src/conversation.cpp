// conversation.cpp

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::~Conversation() {
    delete[] data_;
}

Conversation::Conversation(const Conversation& other)
    : data_(other.size_ > 0 ? new Message[other.size_] : nullptr),
      size_(other.size_),
      capacity_(other.size_) {
    // Copy every message over one at a time so we end up with our own
    // array. Message's own copy assignment handles copying the string
    // content.
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;

    // Simplest way to do this safely: build a full deep copy first,
    // then move it into *this. If the copy throws halfway through, *this 
    // hasn't been modified yet, so nothing gets left in a broken state.
    Conversation temp(other);
    *this = std::move(temp);
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    // Steal the pointer instead of copying elements, then reset other
    // so it's still safe to destroy or assign to later.
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;

    delete[] data_;  // free whatever we currently own first

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::reserve(std::size_t new_capacity) {
    Message* new_data = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        // Move instead of copy here, since we're about to delete the
        // old array anyway, no reason to copy the strings twice.
        new_data[i] = std::move(data_[i]);
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        // Double the capacity whenever we run out of room. Starting at
        // 1 instead of 0 avoids needing a special case for the very
        // first append. Why doubling keeps append O(1) amortized is
        // written up in the design log.
        std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
        reserve(new_capacity);
    }
    data_[size_++] = std::move(m);
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}
