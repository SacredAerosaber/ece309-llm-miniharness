// message.h
// The Message class. It holds who sent a message
// (the role) and what the message actually says (the content).

#pragma once
#include <string>

enum class Role { System, User, Assistant };

class Message {
public:
    // Need a default constructor because Conversation allocates an
    // array of Message objects with "new Message[n]" before it has
    // real content to put in them. This makes a blank one.
    Message() : role_(Role::System), content_("") {}

    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role role() const noexcept { 
        return role_;
    }
    const std::string& content() const noexcept { 
        return content_;
    }

private:
    Role role_;
    std::string content_;
};
