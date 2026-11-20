// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
//
// C ABI of the Rust frontend (frontend/src/lib.rs).
#ifndef KIV_FRONTEND_H
#define KIV_FRONTEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KivBytes {
    uint8_t *data;
    size_t len;
} KivBytes;

// Compiles UTF-8 source text to bytecode.
// Returns 0 and fills *code on success; returns 1 and fills *err with a
// "line:col: message" description on failure. Release both buffers with
// kiv_bytes_free(). Returns 2 if an argument is invalid.
int kiv_compile(const uint8_t *src, size_t src_len, KivBytes *code, KivBytes *err);

// Frees a buffer returned by kiv_compile and resets it to empty. Null is ignored.
void kiv_bytes_free(KivBytes *buf);

#ifdef __cplusplus
}
#endif

#endif // KIV_FRONTEND_H
