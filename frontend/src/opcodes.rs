// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.

//! Opcode values. Must match include/kiv/opcodes.h; the test below checks that.

pub const OP_PUSH_INT: u8 = 0x01;
pub const OP_PUSH_STR: u8 = 0x02;
pub const OP_ADD: u8 = 0x03;
pub const OP_SUB: u8 = 0x04;
pub const OP_MUL: u8 = 0x05;
pub const OP_PRINT: u8 = 0x06;
pub const OP_HALT: u8 = 0x07;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values_match_c_header() {
        let header = include_str!("../../include/kiv/opcodes.h");
        let pairs = [
            ("PUSH_INT", OP_PUSH_INT),
            ("PUSH_STR", OP_PUSH_STR),
            ("ADD", OP_ADD),
            ("SUB", OP_SUB),
            ("MUL", OP_MUL),
            ("PRINT", OP_PRINT),
            ("HALT", OP_HALT),
        ];
        for (name, value) in pairs {
            let needle = format!("KIV_OP_{name} = 0x{value:02X}");
            assert!(header.contains(&needle), "{needle} not found in opcodes.h");
        }
    }
}
