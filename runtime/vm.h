// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#ifndef KIV_RUNTIME_VM_H
#define KIV_RUNTIME_VM_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "heap.h"
#include "value.h"

namespace kiv {

class JitCache;

struct VmOptions {
    bool jit = false;        // compile integer-only expressions to native code
    bool gc_stress = false;  // collect before every allocation
};

class RuntimeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Stack-based bytecode interpreter. Optionally hands integer-only expressions
// to the x86-64 JIT.
class VM {
public:
    VM(std::ostream& out, VmOptions opts);
    ~VM();

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    // Runs one bytecode chunk. Throws RuntimeError on type errors or malformed bytecode.
    void run(const std::vector<uint8_t>& code);

    const Heap& heap() const { return heap_; }

private:
    void binary_op(uint8_t op);
    Value pop();

    Heap heap_;
    std::vector<Value> stack_;
    std::ostream& out_;
    VmOptions opts_;
    std::unique_ptr<JitCache> jit_;
};

} // namespace kiv

#endif // KIV_RUNTIME_VM_H
