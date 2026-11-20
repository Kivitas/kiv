// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
#include "vm.h"

#include <kiv/opcodes.h>

#include <string>

#include "bytecode.h"
#include "jit.h"

namespace kiv {

namespace {
constexpr size_t kInitialGcThreshold = 1u << 20;  // 1 MiB
}

VM::VM(std::ostream& out, VmOptions opts)
    : heap_(kInitialGcThreshold, opts.gc_stress), out_(out), opts_(opts) {
    heap_.set_roots(&stack_);
    if (opts.jit) {
        if (!JitCache::supported()) {
            throw RuntimeError("JIT is not available in this build");
        }
        jit_ = std::make_unique<JitCache>();
    }
}

VM::~VM() = default;

void VM::run(const std::vector<uint8_t>& code) {
    const uint8_t* base = code.data();
    const size_t n = code.size();
    size_t ip = 0;

    stack_.clear();
    if (jit_) {
        jit_->reset();  // the JIT cache is keyed by offset, so it is per chunk
    }

    auto need = [&](size_t bytes) {
        if (ip + bytes > n) {
            throw RuntimeError("truncated bytecode at offset " + std::to_string(ip));
        }
    };

    while (true) {
        if (jit_) {
            // Try to run the whole expression starting here as native code. On
            // success the result is pushed and execution resumes at its PRINT.
            int64_t value = 0;
            size_t print_ip = 0;
            if (jit_->try_run(base, n, ip, value, print_ip)) {
                stack_.push_back(Value::Int(value));
                ip = print_ip;
                continue;
            }
        }

        need(1);
        const uint8_t op = base[ip];
        switch (op) {
            case KIV_OP_PUSH_INT:
                need(9);
                stack_.push_back(Value::Int(read_i64(base + ip + 1)));
                ip += 9;
                break;

            case KIV_OP_PUSH_STR: {
                need(5);
                const uint32_t len = read_u32(base + ip + 1);
                need(5 + static_cast<size_t>(len));
                std::string s(reinterpret_cast<const char*>(base + ip + 5), len);
                stack_.push_back(Value::Str(heap_.new_string(std::move(s))));
                ip += 5 + static_cast<size_t>(len);
                break;
            }

            case KIV_OP_ADD:
            case KIV_OP_SUB:
            case KIV_OP_MUL:
                binary_op(op);
                ip += 1;
                break;

            case KIV_OP_PRINT: {
                const Value v = pop();
                if (v.tag == Tag::Int) {
                    out_ << v.i;
                } else {
                    out_ << v.as_string();
                }
                out_ << '\n';
                ip += 1;
                break;
            }

            case KIV_OP_HALT:
                return;

            default:
                throw RuntimeError("unknown opcode " + std::to_string(op) + " at offset " +
                                   std::to_string(ip));
        }
    }
}

void VM::binary_op(uint8_t op) {
    if (stack_.size() < 2) {
        throw RuntimeError("stack underflow");
    }
    const size_t top = stack_.size();
    // Operands stay on the stack until the result exists, so the collector can see them.
    const Value a = stack_[top - 2];
    const Value b = stack_[top - 1];

    Value result = Value::Int(0);
    if (a.tag == Tag::Int && b.tag == Tag::Int) {
        // Unsigned arithmetic wraps well-defined, matching the JIT's machine instructions.
        const uint64_t x = static_cast<uint64_t>(a.i);
        const uint64_t y = static_cast<uint64_t>(b.i);
        uint64_t r = 0;
        if (op == KIV_OP_ADD) {
            r = x + y;
        } else if (op == KIV_OP_SUB) {
            r = x - y;
        } else {
            r = x * y;
        }
        result = Value::Int(static_cast<int64_t>(r));
    } else if (op == KIV_OP_ADD && a.tag == Tag::Str && b.tag == Tag::Str) {
        result = Value::Str(heap_.new_string(a.as_string() + b.as_string()));
    } else {
        throw RuntimeError("type error: operands do not support this operation");
    }

    stack_.resize(top - 2);
    stack_.push_back(result);
}

Value VM::pop() {
    if (stack_.empty()) {
        throw RuntimeError("stack underflow");
    }
    const Value v = stack_.back();
    stack_.pop_back();
    return v;
}

} // namespace kiv
