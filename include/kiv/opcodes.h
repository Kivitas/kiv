// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
//
// Bytecode format shared by the Rust frontend and the C++ runtime.
// Keep this in sync with frontend/src/opcodes.rs (a unit test checks it).
#ifndef KIV_OPCODES_H
#define KIV_OPCODES_H

#include <cstdint>

// Immediates are little-endian regardless of host byte order.
enum KivOp : uint8_t {
    KIV_OP_PUSH_INT = 0x01, // i64 immediate (8 bytes)
    KIV_OP_PUSH_STR = 0x02, // u32 length (4 bytes), then that many UTF-8 bytes
    KIV_OP_ADD = 0x03,      // int+int, or str+str (concatenation)
    KIV_OP_SUB = 0x04,      // int-int
    KIV_OP_MUL = 0x05,      // int*int
    KIV_OP_PRINT = 0x06,    // pops one value and writes it plus a newline
    KIV_OP_HALT = 0x07,     // end of program
};

#endif // KIV_OPCODES_H
