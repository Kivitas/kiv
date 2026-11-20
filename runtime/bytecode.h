// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#ifndef KIV_RUNTIME_BYTECODE_H
#define KIV_RUNTIME_BYTECODE_H

#include <cstddef>
#include <cstdint>

namespace kiv {

// Bytecode immediates are little-endian; decode byte-by-byte so the
// runtime is correct on any host byte order.
inline int64_t read_i64(const uint8_t* p) {
    uint64_t v = 0;
    for (int k = 7; k >= 0; --k) {
        v = (v << 8) | p[k];
    }
    return static_cast<int64_t>(v);
}

inline uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace kiv

#endif // KIV_RUNTIME_BYTECODE_H
