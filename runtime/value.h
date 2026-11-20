// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#ifndef KIV_RUNTIME_VALUE_H
#define KIV_RUNTIME_VALUE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace kiv {

// Base of every garbage-collected object. Objects form an intrusive list
// owned by the Heap.
struct Object {
    Object* next = nullptr;
    size_t bytes = 0;     // accounted size, used for GC pacing
    bool marked = false;  // set during the mark phase
    virtual ~Object() = default;
};

struct StringObject final : Object {
    std::string data;

    explicit StringObject(std::string s) : data(std::move(s)) {
        bytes = sizeof(*this) + data.size();
    }
};

enum class Tag : uint8_t { Int, Str };

// A runtime value: an unboxed 64-bit integer or a reference to a heap string.
struct Value {
    Tag tag;
    union {
        int64_t i;
        Object* obj;
    };

    static Value Int(int64_t v) {
        Value r{};
        r.tag = Tag::Int;
        r.i = v;
        return r;
    }

    static Value Str(StringObject* s) {
        Value r{};
        r.tag = Tag::Str;
        r.obj = s;
        return r;
    }

    const std::string& as_string() const {
        return static_cast<const StringObject*>(obj)->data;
    }
};

} // namespace kiv

#endif // KIV_RUNTIME_VALUE_H
