// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#include "heap.h"

#include <algorithm>
#include <utility>

namespace kiv {

Heap::Heap(size_t initial_threshold_bytes, bool stress)
    : threshold_(initial_threshold_bytes),
      initial_threshold_(initial_threshold_bytes),
      stress_(stress) {}

Heap::~Heap() {
    while (objects_ != nullptr) {
        Object* next = objects_->next;
        delete objects_;
        objects_ = next;
    }
}

StringObject* Heap::new_string(std::string s) {
    // Collect first: the new object is not yet reachable from any root.
    if (stress_ || bytes_ >= threshold_) {
        collect();
    }
    auto* obj = new StringObject(std::move(s));
    obj->next = objects_;
    objects_ = obj;
    bytes_ += obj->bytes;
    return obj;
}

void Heap::mark(const Value& v) {
    if (v.tag != Tag::Str || v.obj == nullptr || v.obj->marked) {
        return;
    }
    // Strings hold no references. A container type would push its children
    // onto a worklist here.
    v.obj->marked = true;
}

void Heap::sweep() {
    Object** link = &objects_;
    while (*link != nullptr) {
        Object* obj = *link;
        if (obj->marked) {
            obj->marked = false;
            link = &obj->next;
        } else {
            *link = obj->next;
            bytes_ -= obj->bytes;
            stats_.freed++;
            delete obj;
        }
    }
}

void Heap::collect() {
    if (roots_ != nullptr) {
        for (const Value& v : *roots_) {
            mark(v);
        }
    }
    sweep();
    stats_.collections++;
    stats_.live_bytes = bytes_;
    // Next collection after the heap doubles, but never below the initial threshold.
    threshold_ = std::max(initial_threshold_, bytes_ * 2);
}

} // namespace kiv
