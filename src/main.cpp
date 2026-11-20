// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 kivitas
// Licensed under the GNU GPL v3 or later; see the LICENSE file.
//
// Command-line driver: kiv [--jit] [--gc-stress] [--gc-stats] <script.kiv>

#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <kiv/frontend.h>

#include "jit.h"
#include "vm.h"

namespace {

// Exit codes (sysexits-style where one applies).
constexpr int kExitCompileError = 1;
constexpr int kExitRuntimeError = 2;
constexpr int kExitUsage = 64;
constexpr int kExitNoInput = 66;

void usage(std::ostream& os) {
    os << "usage: kiv [--jit] [--gc-stress] [--gc-stats] <script.kiv>\n"
          "  --jit        compile integer expressions to x86-64 machine code\n"
          "  --gc-stress  run the garbage collector before every allocation\n"
          "  --gc-stats   print collector statistics to stderr\n";
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    kiv::VmOptions opts;
    bool show_stats = false;
    std::string path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--jit") {
            opts.jit = true;
        } else if (arg == "--gc-stress") {
            opts.gc_stress = true;
        } else if (arg == "--gc-stats") {
            show_stats = true;
        } else if (arg == "-h" || arg == "--help") {
            usage(std::cout);
            return 0;
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "kiv: unknown option " << arg << "\n";
            usage(std::cerr);
            return kExitUsage;
        } else if (path.empty()) {
            path = arg;
        } else {
            usage(std::cerr);
            return kExitUsage;
        }
    }

    if (path.empty()) {
        usage(std::cerr);
        return kExitUsage;
    }
    if (opts.jit && !kiv::JitCache::supported()) {
        std::cerr << "kiv: --jit needs an x86-64 Linux build\n";
        return kExitUsage;
    }

    std::string source;
    if (!read_file(path, source)) {
        std::cerr << "kiv: cannot read " << path << "\n";
        return kExitNoInput;
    }

    KivBytes code{nullptr, 0};
    KivBytes err{nullptr, 0};
    const int rc = kiv_compile(reinterpret_cast<const uint8_t*>(source.data()), source.size(),
                               &code, &err);
    if (rc != 0) {
        std::cerr << path << ":" << std::string(reinterpret_cast<const char*>(err.data), err.len)
                  << "\n";
        kiv_bytes_free(&err);
        return kExitCompileError;
    }
    const std::vector<uint8_t> bytecode(code.data, code.data + code.len);
    kiv_bytes_free(&code);

    kiv::VM vm(std::cout, opts);
    try {
        vm.run(bytecode);
    } catch (const std::exception& e) {
        std::cout.flush();
        std::cerr << path << ": runtime error: " << e.what() << "\n";
        return kExitRuntimeError;
    }
    std::cout.flush();

    if (show_stats) {
        const auto& s = vm.heap().stats();
        std::cerr << "gc: " << s.collections << " collections, " << s.freed
                  << " objects freed, " << s.live_bytes << " live bytes\n";
    }
    return 0;
}
