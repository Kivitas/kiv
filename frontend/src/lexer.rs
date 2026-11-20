// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.

//! Turns source text into tokens with line and column positions.

use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Print,
    Int(i64),
    Str(String),
    Plus,
    Minus,
    Star,
    LParen,
    RParen,
    Semi,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Spanned {
    pub tok: Token,
    pub line: usize,
    pub col: usize,
}

#[derive(Debug, Clone, PartialEq)]
pub struct LexError {
    pub line: usize,
    pub col: usize,
    pub msg: String,
}

impl fmt::Display for LexError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: {}", self.line, self.col, self.msg)
    }
}

struct Lexer<'a> {
    chars: std::iter::Peekable<std::str::Chars<'a>>,
    line: usize,
    col: usize,
}

impl Lexer<'_> {
    fn bump(&mut self) -> Option<char> {
        let c = self.chars.next()?;
        if c == '\n' {
            self.line += 1;
            self.col = 1;
        } else {
            self.col += 1;
        }
        Some(c)
    }

    fn peek(&mut self) -> Option<char> {
        self.chars.peek().copied()
    }
}

pub fn tokenize(src: &str) -> Result<Vec<Spanned>, LexError> {
    let mut lx = Lexer {
        chars: src.chars().peekable(),
        line: 1,
        col: 1,
    };
    let mut out = Vec::new();

    while let Some(c) = lx.peek() {
        let (line, col) = (lx.line, lx.col);
        let err = |msg: String| LexError { line, col, msg };

        let tok = match c {
            c if c.is_whitespace() => {
                lx.bump();
                continue;
            }
            '#' => {
                // Comment: skip to end of line.
                while let Some(ch) = lx.peek() {
                    if ch == '\n' {
                        break;
                    }
                    lx.bump();
                }
                continue;
            }
            '+' => {
                lx.bump();
                Token::Plus
            }
            '-' => {
                lx.bump();
                Token::Minus
            }
            '*' => {
                lx.bump();
                Token::Star
            }
            '(' => {
                lx.bump();
                Token::LParen
            }
            ')' => {
                lx.bump();
                Token::RParen
            }
            ';' => {
                lx.bump();
                Token::Semi
            }
            '"' => {
                lx.bump();
                let mut s = String::new();
                loop {
                    match lx.bump() {
                        Some('"') => break,
                        Some(ch) => s.push(ch),
                        None => return Err(err("unterminated string literal".into())),
                    }
                }
                Token::Str(s)
            }
            '0'..='9' => {
                let mut n: i64 = 0;
                while let Some(d) = lx.peek().and_then(|ch| ch.to_digit(10)) {
                    lx.bump();
                    n = n
                        .checked_mul(10)
                        .and_then(|n| n.checked_add(i64::from(d)))
                        .ok_or_else(|| err("integer literal out of range".into()))?;
                }
                Token::Int(n)
            }
            c if c.is_alphabetic() => {
                let mut word = String::new();
                while let Some(ch) = lx.peek().filter(|ch| ch.is_alphanumeric() || *ch == '_') {
                    word.push(ch);
                    lx.bump();
                }
                match word.as_str() {
                    "print" => Token::Print,
                    other => return Err(err(format!("unknown keyword '{other}'"))),
                }
            }
            other => return Err(err(format!("unexpected character '{other}'"))),
        };

        out.push(Spanned { tok, line, col });
    }

    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tokenizes_statement() {
        let toks = tokenize("print (2);").unwrap();
        let kinds: Vec<Token> = toks.into_iter().map(|s| s.tok).collect();
        assert_eq!(
            kinds,
            vec![
                Token::Print,
                Token::LParen,
                Token::Int(2),
                Token::RParen,
                Token::Semi
            ]
        );
    }

    #[test]
    fn tracks_lines_across_newlines() {
        let toks = tokenize("print\n  1;").unwrap();
        assert_eq!((toks[1].line, toks[1].col), (2, 3));
    }
}
