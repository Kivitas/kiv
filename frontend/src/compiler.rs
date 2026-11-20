// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.

//! Lowers the AST to the stack bytecode described in include/kiv/opcodes.h.

use crate::opcodes::*;
use crate::parser::{BinOp, Expr, Program, Stmt};

pub fn compile(program: &Program) -> Result<Vec<u8>, String> {
    let mut code = Vec::new();
    for stmt in program {
        match stmt {
            Stmt::Print(expr) => {
                emit_expr(expr, &mut code)?;
                code.push(OP_PRINT);
            }
        }
    }
    code.push(OP_HALT);
    Ok(code)
}

fn emit_expr(expr: &Expr, code: &mut Vec<u8>) -> Result<(), String> {
    match expr {
        Expr::Int(n) => {
            code.push(OP_PUSH_INT);
            code.extend_from_slice(&n.to_le_bytes());
        }
        Expr::Str(s) => {
            let len = u32::try_from(s.len())
                .map_err(|_| "string literal longer than 4 GiB".to_string())?;
            code.push(OP_PUSH_STR);
            code.extend_from_slice(&len.to_le_bytes());
            code.extend_from_slice(s.as_bytes());
        }
        Expr::Binary { op, lhs, rhs } => {
            emit_expr(lhs, code)?;
            emit_expr(rhs, code)?;
            code.push(match op {
                BinOp::Add => OP_ADD,
                BinOp::Sub => OP_SUB,
                BinOp::Mul => OP_MUL,
            });
        }
    }
    Ok(())
}
