// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#ifndef KIV_RUNTIME_HEAP_H
#define KIV_RUNTIME_HEAP_H

#include <cstddef>
#include <string>
#include <vector>

#include "value.h"

namespace kiv {

// Non-moving mark-and-sweep collector.
//
// Roots are the VM value stack, registered with set_roots(). Collection runs
// before an allocation, so every live value must already be on the stack.
class Heap {
public:
    struct Stats {
        size_t collections = 0;
        size_t freed = 0;
        size_t live_bytes = 0;  // as of the most recent collection
    };

    // stress = true collects before every allocation (useful for testing).
    Heap(size_t initial_threshold_bytes, bool stress);
    ~Heap();

    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;

    void set_roots(const std::vector<Value>* roots) { roots_ = roots; }

    StringObject* new_string(std::string s);
    void collect();

    const Stats& stats() const { return stats_; }

private:
    void mark(const Value& v);
    void sweep();

    Object* objects_ = nullptr;
    const std::vector<Value>* roots_ = nullptr;
    size_t bytes_ = 0;
    size_t threshold_;
    size_t initial_threshold_;
    bool stress_;
    Stats stats_;
};

} // namespace kiv

#endif // KIV_RUNTIME_HEAP_H
