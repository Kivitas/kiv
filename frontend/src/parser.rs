// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.

//! Recursive-descent parser producing a small AST.
//!
//! Grammar (v0):
//!   program  := stmt*
//!   stmt     := 'print' expr ';'
//!   expr     := term (('+' | '-') term)*
//!   term     := primary ('*' primary)*
//!   primary  := INT | STRING | '(' expr ')'

use std::fmt;

use crate::lexer::{Spanned, Token};

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Int(i64),
    Str(String),
    Binary {
        op: BinOp,
        lhs: Box<Expr>,
        rhs: Box<Expr>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum Stmt {
    Print(Expr),
}

pub type Program = Vec<Stmt>;

#[derive(Debug, Clone, PartialEq)]
pub struct ParseError {
    pub line: usize,
    pub col: usize,
    pub msg: String,
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: {}", self.line, self.col, self.msg)
    }
}

struct Parser<'a> {
    toks: &'a [Spanned],
    pos: usize,
}

pub fn parse(tokens: &[Spanned]) -> Result<Program, ParseError> {
    let mut p = Parser {
        toks: tokens,
        pos: 0,
    };
    let mut program = Vec::new();
    while p.pos < tokens.len() {
        program.push(p.statement()?);
    }
    Ok(program)
}

impl<'a> Parser<'a> {
    fn peek(&self) -> Option<&'a Token> {
        self.toks.get(self.pos).map(|s| &s.tok)
    }

    fn error<T>(&self, msg: String) -> Result<T, ParseError> {
        let (line, col) = match self.toks.get(self.pos).or(self.toks.last()) {
            Some(t) => (t.line, t.col),
            None => (1, 1),
        };
        Err(ParseError { line, col, msg })
    }

    fn expect(&mut self, want: &Token, what: &str) -> Result<(), ParseError> {
        if self.peek() == Some(want) {
            self.pos += 1;
            Ok(())
        } else {
            self.error(format!("expected {what}"))
        }
    }

    fn statement(&mut self) -> Result<Stmt, ParseError> {
        self.expect(&Token::Print, "'print'")?;
        let e = self.expr()?;
        self.expect(&Token::Semi, "';' after expression")?;
        Ok(Stmt::Print(e))
    }

    fn expr(&mut self) -> Result<Expr, ParseError> {
        let mut lhs = self.term()?;
        loop {
            let op = match self.peek() {
                Some(Token::Plus) => BinOp::Add,
                Some(Token::Minus) => BinOp::Sub,
                _ => break,
            };
            self.pos += 1;
            let rhs = self.term()?;
            lhs = Expr::Binary {
                op,
                lhs: Box::new(lhs),
                rhs: Box::new(rhs),
            };
        }
        Ok(lhs)
    }

    fn term(&mut self) -> Result<Expr, ParseError> {
        let mut lhs = self.primary()?;
        while let Some(Token::Star) = self.peek() {
            self.pos += 1;
            let rhs = self.primary()?;
            lhs = Expr::Binary {
                op: BinOp::Mul,
                lhs: Box::new(lhs),
                rhs: Box::new(rhs),
            };
        }
        Ok(lhs)
    }

    fn primary(&mut self) -> Result<Expr, ParseError> {
        match self.peek() {
            Some(Token::Int(n)) => {
                self.pos += 1;
                Ok(Expr::Int(*n))
            }
            Some(Token::Str(s)) => {
                self.pos += 1;
                Ok(Expr::Str(s.clone()))
            }
            Some(Token::LParen) => {
                self.pos += 1;
                let e = self.expr()?;
                self.expect(&Token::RParen, "')'")?;
                Ok(e)
            }
            _ => self.error("expected an integer, string or '('".to_string()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::tokenize;

    #[test]
    fn multiplication_binds_tighter_than_addition() {
        let prog = parse(&tokenize("print 1 + 2 * 3;").unwrap()).unwrap();
        let expected = Stmt::Print(Expr::Binary {
            op: BinOp::Add,
            lhs: Box::new(Expr::Int(1)),
            rhs: Box::new(Expr::Binary {
                op: BinOp::Mul,
                lhs: Box::new(Expr::Int(2)),
                rhs: Box::new(Expr::Int(3)),
            }),
        });
        assert_eq!(prog, vec![expected]);
    }

    #[test]
    fn addition_is_left_associative() {
        let prog = parse(&tokenize("print 10 - 4 - 3;").unwrap()).unwrap();
        let Stmt::Print(Expr::Binary { lhs, .. }) = &prog[0] else {
            panic!("expected a binary print statement");
        };
        assert!(matches!(**lhs, Expr::Binary { op: BinOp::Sub, .. }));
    }

    #[test]
    fn missing_semicolon_is_an_error() {
        let err = parse(&tokenize("print 1").unwrap()).unwrap_err();
        assert_eq!(err.msg, "expected ';' after expression");
    }
}
