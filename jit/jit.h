// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#ifndef KIV_JIT_JIT_H
#define KIV_JIT_JIT_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kiv {

// Compiles integer-only expressions to native x86-64 code.
//
// A region starts at a bytecode offset and covers PUSH_INT, ADD, SUB and MUL
// up to the PRINT that consumes the expression's single result. Any other
// opcode means the region is left to the interpreter.
//
// Only x86-64 Linux builds have a real JIT (see KIV_ENABLE_JIT). Elsewhere
// supported() is false and try_run() never succeeds.
class JitCache {
public:
    JitCache();
    ~JitCache();

    JitCache(const JitCache&) = delete;
    JitCache& operator=(const JitCache&) = delete;

    static bool supported();

    // Drops all compiled code. Call before running a new bytecode chunk.
    void reset();

    // If an integer expression starts at `ip`, runs it natively and stores its
    // value and the offset of the PRINT that follows it. Returns false otherwise.
    bool try_run(const uint8_t* code, size_t len, size_t ip, int64_t& value, size_t& print_ip);

    size_t compiled_regions() const { return compiled_; }

private:
    struct Region {
        void* entry = nullptr;  // null means "not compilable"; cached so we do not retry
        size_t print_ip = 0;
    };

    Region compile(const uint8_t* code, size_t len, size_t ip);
    void* install(const std::vector<uint8_t>& machine_code);

    std::unordered_map<size_t, Region> regions_;
    std::vector<std::pair<void*, size_t>> mappings_;  // (base, size) of each executable page run
    size_t compiled_ = 0;
};

} // namespace kiv

#endif // KIV_JIT_JIT_H
