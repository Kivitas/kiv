// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.

//! Frontend for kiv: source text in, bytecode out.
//!
//! Pipeline: `lexer::tokenize` -> `parser::parse` -> `compiler::compile`.
//! The C++ runtime reaches it through the `kiv_compile` C ABI.

pub mod compiler;
pub mod lexer;
pub mod opcodes;
pub mod parser;

use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

/// Compiles source text to bytecode. Errors are formatted as "line:col: message".
pub fn compile_source(src: &str) -> Result<Vec<u8>, String> {
    let tokens = lexer::tokenize(src).map_err(|e| e.to_string())?;
    let program = parser::parse(&tokens).map_err(|e| e.to_string())?;
    compiler::compile(&program)
}

/// Buffer handed across the C ABI. Allocated here, released by `kiv_bytes_free`.
#[repr(C)]
pub struct KivBytes {
    pub data: *mut u8,
    pub len: usize,
}

impl KivBytes {
    const EMPTY: KivBytes = KivBytes {
        data: ptr::null_mut(),
        len: 0,
    };

    fn from_vec(v: Vec<u8>) -> KivBytes {
        let boxed = v.into_boxed_slice();
        let len = boxed.len();
        KivBytes {
            data: Box::into_raw(boxed) as *mut u8,
            len,
        }
    }
}

/// Compiles UTF-8 source to bytecode across the C ABI.
///
/// # Safety
/// `src` must be readable for `src_len` bytes (or may be null when `src_len` is 0),
/// and `code` and `err` must be valid for writes.
#[no_mangle]
pub unsafe extern "C" fn kiv_compile(
    src: *const u8,
    src_len: usize,
    code: *mut KivBytes,
    err: *mut KivBytes,
) -> i32 {
    if code.is_null() || err.is_null() || (src.is_null() && src_len != 0) {
        return 2;
    }
    *code = KivBytes::EMPTY;
    *err = KivBytes::EMPTY;

    let source: &[u8] = if src_len == 0 {
        &[]
    } else {
        slice::from_raw_parts(src, src_len)
    };

    // A panic must never unwind into C++.
    let result = catch_unwind(AssertUnwindSafe(|| match std::str::from_utf8(source) {
        Ok(text) => compile_source(text),
        Err(_) => Err("source is not valid UTF-8".to_string()),
    }));

    match result {
        Ok(Ok(bytes)) => {
            *code = KivBytes::from_vec(bytes);
            0
        }
        Ok(Err(msg)) => {
            *err = KivBytes::from_vec(msg.into_bytes());
            1
        }
        Err(_) => {
            *err = KivBytes::from_vec(b"internal compiler error".to_vec());
            1
        }
    }
}

/// Frees a buffer produced by `kiv_compile`.
///
/// # Safety
/// `buf` must be null or point to a `KivBytes` written by `kiv_compile`.
#[no_mangle]
pub unsafe extern "C" fn kiv_bytes_free(buf: *mut KivBytes) {
    if buf.is_null() {
        return;
    }
    let b = &mut *buf;
    if !b.data.is_null() {
        drop(Box::from_raw(ptr::slice_from_raw_parts_mut(b.data, b.len)));
    }
    *b = KivBytes::EMPTY;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::opcodes::*;

    #[test]
    fn precedence_and_bytecode_layout() {
        let code = compile_source("print 1 + 2 * 3;").unwrap();
        let mut expected = vec![OP_PUSH_INT];
        expected.extend(1i64.to_le_bytes());
        expected.push(OP_PUSH_INT);
        expected.extend(2i64.to_le_bytes());
        expected.push(OP_PUSH_INT);
        expected.extend(3i64.to_le_bytes());
        expected.push(OP_MUL);
        expected.push(OP_ADD);
        expected.push(OP_PRINT);
        expected.push(OP_HALT);
        assert_eq!(code, expected);
    }

    #[test]
    fn string_literal_is_length_prefixed() {
        let code = compile_source("print \"hi\";").unwrap();
        let mut expected = vec![OP_PUSH_STR];
        expected.extend(2u32.to_le_bytes());
        expected.extend(b"hi");
        expected.push(OP_PRINT);
        expected.push(OP_HALT);
        assert_eq!(code, expected);
    }

    #[test]
    fn comments_and_empty_program() {
        assert_eq!(compile_source("# nothing here\n").unwrap(), vec![OP_HALT]);
    }

    #[test]
    fn syntax_error_reports_position() {
        let err = compile_source("print 1 +;").unwrap_err();
        assert_eq!(err, "1:10: expected an integer, string or '('");
    }

    #[test]
    fn unknown_character_reports_position() {
        let err = compile_source("print 1 $ 2;").unwrap_err();
        assert_eq!(err, "1:9: unexpected character '$'");
    }

    #[test]
    fn integer_overflow_is_a_lex_error() {
        let err = compile_source("print 9223372036854775808;").unwrap_err();
        assert_eq!(err, "1:7: integer literal out of range");
    }
}
