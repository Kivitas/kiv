// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#include "jit.h"

#include <kiv/opcodes.h>

#include "bytecode.h"

#if defined(KIV_ENABLE_JIT)

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <initializer_list>
#include <stdexcept>

// Defined in trampoline_x86_64.S.
extern "C" int64_t kiv_jit_call(const void* entry);

namespace kiv {

namespace {

void emit(std::vector<uint8_t>& out, std::initializer_list<uint8_t> bytes) {
    out.insert(out.end(), bytes);
}

// Translates the expression at `ip` into x86-64 machine code.
//
// Calling convention of the generated code: no arguments, the result comes
// back in rax. Intermediate values live on the native stack. Generated code
// makes no calls, so it needs no stack frame and no special alignment.
bool translate(const uint8_t* code, size_t len, size_t ip, std::vector<uint8_t>& out,
               size_t& print_ip) {
    int depth = 0;
    size_t i = ip;
    while (i < len) {
        switch (code[i]) {
            case KIV_OP_PUSH_INT: {
                if (i + 9 > len) {
                    return false;
                }
                const uint64_t imm = static_cast<uint64_t>(read_i64(code + i + 1));
                emit(out, {0x48, 0xB8});  // mov rax, imm64
                for (int k = 0; k < 8; ++k) {
                    out.push_back(static_cast<uint8_t>(imm >> (8 * k)));
                }
                emit(out, {0x50});  // push rax
                ++depth;
                i += 9;
                break;
            }

            case KIV_OP_ADD:
            case KIV_OP_SUB:
            case KIV_OP_MUL: {
                if (depth < 2) {
                    return false;
                }
                emit(out, {0x59, 0x58});  // pop rcx (rhs) ; pop rax (lhs)
                switch (code[i]) {
                    case KIV_OP_ADD:
                        emit(out, {0x48, 0x01, 0xC8});  // add rax, rcx
                        break;
                    case KIV_OP_SUB:
                        emit(out, {0x48, 0x29, 0xC8});  // sub rax, rcx
                        break;
                    default:
                        emit(out, {0x48, 0x0F, 0xAF, 0xC1});  // imul rax, rcx
                        break;
                }
                emit(out, {0x50});  // push rax
                --depth;
                ++i;
                break;
            }

            case KIV_OP_PRINT:
                // Only a complete expression, with exactly one value left, can be compiled.
                if (depth != 1) {
                    return false;
                }
                emit(out, {0x58, 0xC3});  // pop rax ; ret
                print_ip = i;
                return true;

            default:
                // Strings, HALT and anything else stay with the interpreter.
                return false;
        }
    }
    return false;
}

} // namespace

JitCache::JitCache() = default;

JitCache::~JitCache() { reset(); }

bool JitCache::supported() { return true; }

void JitCache::reset() {
    for (const auto& m : mappings_) {
        munmap(m.first, m.second);
    }
    mappings_.clear();
    regions_.clear();
    compiled_ = 0;
}

JitCache::Region JitCache::compile(const uint8_t* code, size_t len, size_t ip) {
    std::vector<uint8_t> machine_code;
    size_t print_ip = 0;
    Region region;
    if (!translate(code, len, ip, machine_code, print_ip)) {
        return region;  // entry stays null
    }
    region.entry = install(machine_code);
    region.print_ip = print_ip;
    ++compiled_;
    return region;
}

// Copies machine code into fresh pages and makes them executable. The pages
// are never writable and executable at the same time (W^X).
void* JitCache::install(const std::vector<uint8_t>& machine_code) {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t size = (machine_code.size() + page - 1) / page * page;

    void* mem = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        throw std::runtime_error("jit: mmap failed");
    }
    std::memcpy(mem, machine_code.data(), machine_code.size());
    if (mprotect(mem, size, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, size);
        throw std::runtime_error("jit: mprotect failed");
    }
    mappings_.emplace_back(mem, size);
    return mem;
}

bool JitCache::try_run(const uint8_t* code, size_t len, size_t ip, int64_t& value,
                       size_t& print_ip) {
    auto it = regions_.find(ip);
    if (it == regions_.end()) {
        it = regions_.emplace(ip, compile(code, len, ip)).first;
    }
    if (it->second.entry == nullptr) {
        return false;
    }
    value = kiv_jit_call(it->second.entry);
    print_ip = it->second.print_ip;
    return true;
}

} // namespace kiv

#else // !KIV_ENABLE_JIT

namespace kiv {

JitCache::JitCache() = default;
JitCache::~JitCache() = default;

bool JitCache::supported() { return false; }

void JitCache::reset() {}

JitCache::Region JitCache::compile(const uint8_t*, size_t, size_t) { return Region{}; }

void* JitCache::install(const std::vector<uint8_t>&) { return nullptr; }

bool JitCache::try_run(const uint8_t*, size_t, size_t, int64_t&, size_t&) { return false; }

} // namespace kiv

#endif // KIV_ENABLE_JIT
