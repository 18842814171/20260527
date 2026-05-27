#ifndef RUN_TEST_SIMULATOR_HPP
#define RUN_TEST_SIMULATOR_HPP

#include "simulator.hpp"
#include "utils/utils.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

/** 解析 --debug=0|1、--trap-uart=0|1；环境变量 SIM_TRAP_UART 非空时默认开启 trap UART 镜像。 */
inline void apply_debug_cli_flags(int argc, char** argv) {
    constexpr const char kDebugPrefix[] = "--debug=";
    constexpr const char kTrapPrefix[] = "--trap-uart=";
    const size_t debug_plen = sizeof(kDebugPrefix) - 1;
    const size_t trap_plen = sizeof(kTrapPrefix) - 1;

    const char* env_trap = std::getenv("SIM_TRAP_UART");
    if (env_trap && env_trap[0] != '\0') {
        sim_trap_uart_trace_enabled() = true;
    }

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a.size() > debug_plen && a.compare(0, debug_plen, kDebugPrefix) == 0) {
            const std::string val = a.substr(debug_plen);
            if (val == "0")
                sim_debug_runtime_enabled() = false;
            else if (val == "1")
                sim_debug_runtime_enabled() = true;
            else
                std::cerr << "警告: " << a << " 无效，请使用 --debug=0 或 --debug=1\n";
            continue;
        }
        if (a.size() > trap_plen && a.compare(0, trap_plen, kTrapPrefix) == 0) {
            const std::string val = a.substr(trap_plen);
            if (val == "0")
                sim_trap_uart_trace_enabled() = false;
            else if (val == "1")
                sim_trap_uart_trace_enabled() = true;
            else
                std::cerr << "警告: " << a << " 无效，请使用 --trap-uart=0 或 --trap-uart=1\n";
            continue;
        }
        if (a == "--trap-uart" || a == "--trap_uart") {
            sim_trap_uart_trace_enabled() = true;
            continue;
        }
        if (a.size() > trap_plen && a.compare(0, trap_plen, "--trap_uart=") == 0) {
            const std::string val = a.substr(sizeof("--trap_uart=") - 1);
            if (val == "0")
                sim_trap_uart_trace_enabled() = false;
            else if (val == "1")
                sim_trap_uart_trace_enabled() = true;
            else
                std::cerr << "警告: " << a << " 无效，请使用 --trap-uart=1\n";
            std::cerr << "提示: 参数名是 --trap-uart（连字符），不是 --trap_uart\n";
        }
    }
}

inline bool arg_is_cli_flag(const char* s) {
    return s && std::strncmp(s, "--", 2) == 0;
}

inline std::string resolve_input_path_with_defaults(const std::string& raw,
                                                    const std::vector<std::string>& prefixes,
                                                    const std::vector<std::string>& suffixes = {""}) {
    namespace fs = std::filesystem;
    if (raw.empty()) {
        return raw;
    }
    fs::path p(raw);
    if (p.is_absolute() || raw.find('/') != std::string::npos) {
        return raw;
    }
    for (const auto& sfx : suffixes) {
        const std::string direct = raw + sfx;
        if (fs::exists(direct)) {
            return direct;
        }
    }
    for (const auto& pre : prefixes) {
        for (const auto& sfx : suffixes) {
            const fs::path candidate = fs::path(pre) / (raw + sfx);
            if (fs::exists(candidate)) {
                return candidate.string();
            }
        }
    }
    return raw;
}

/** 解析 --log-steps=<n> 或 --log-steps <n>；0 表示不限制日志步数。 */
inline size_t parse_log_steps_from_argv(int argc, char** argv) {
    constexpr const char kPrefix[] = "--log-steps=";
    const size_t plen = sizeof(kPrefix) - 1;
    for (int i = 2; i < argc; ++i) {
        std::string a(argv[i]);
        std::string value_str;
        if (a.size() > plen && a.compare(0, plen, kPrefix) == 0) {
            value_str = a.substr(plen);
        } else if (a == "--log-steps") {
            if (i + 1 >= argc) {
                std::cerr << "警告: --log-steps 缺少数值，已忽略\n";
                return std::numeric_limits<size_t>::max();
            }
            value_str = std::string(argv[++i]);
        } else {
            continue;
        }
        try {
            const unsigned long long v = std::stoull(value_str);
            if (v == 0) {
                return std::numeric_limits<size_t>::max();
            }
            return static_cast<size_t>(v);
        } catch (...) {
            std::cerr << "警告: --log-steps 参数无效: " << value_str << "，已忽略\n";
            return std::numeric_limits<size_t>::max();
        }
    }
    return std::numeric_limits<size_t>::max();
}

/** 在 argv[from..argc) 中取第一个可解析的正整数为 max_steps（跳过 -- 开头项）。 */
inline size_t parse_max_steps_from_argv(int argc, char** argv, int from) {
    for (int i = from; i < argc; ++i) {
        const std::string a(argv[i]);
        if (a == "--log-steps") {
            ++i; // Skip log-steps value token.
            continue;
        }
        constexpr const char kLogEq[] = "--log-steps=";
        if (a.compare(0, sizeof(kLogEq) - 1, kLogEq) == 0) {
            continue;
        }
        if (arg_is_cli_flag(argv[i]))
            continue;
        try {
            unsigned long long v = std::stoull(argv[i]);
            if (v == 0) {
                std::cerr << "max_steps 须为正整数，已忽略: " << argv[i] << "\n";
                continue;
            }
            return static_cast<size_t>(v);
        } catch (...) {
            continue;
        }
    }
    return 0;
}

/** 测试入口：LogRedirector 写 log/ 下文件，并抑制 UART MMIO 的调试行（见 SimLogConfig）。 */
inline int run_test_simulator_cli(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <elf> [max_steps] [--debug=0|1] [--trap-uart=0|1] [--log-steps=<n>]\n"
                  << "  max_steps 为正整数可选；可与 --debug 混排。\n"
                  << "  <elf> 支持仅填文件名：自动尝试当前目录、out/、tests/。\n"
                  << "  --log-steps=<n> 仅限制日志记录前 n 个 step；不影响实际运行结束条件（0=不限制）。\n"
                  << "  --debug=0  安静模式：不写 log/ 文件、不输出 LOG/调试类信息（stderr 仅保留告警/错误）。\n"
                  << "  --debug=1  显式开启调试输出（默认即为开启）。\n"
                  << "  --trap-uart=1  将 [Interrupt] TRAP 行镜像到终端（与 guest UART 同 stdout）；或设 SIM_TRAP_UART=1。\n"
                  << "  -h, --help  显示本帮助\n";
        return 1;
    }

    apply_debug_cli_flags(argc, argv);

    std::string a1(argv[1]);
    if (a1 == "-h" || a1 == "--help") {
        std::cout << "用法: " << argv[0] << " <elf> [max_steps] [--debug=0|1] [--log-steps=<n>]\n";
        return 0;
    }

    std::string elf;
    size_t max_steps = 0;

    elf = resolve_input_path_with_defaults(a1, {"out", "tests"});
    max_steps = parse_max_steps_from_argv(argc, argv, 2);
    SimLogConfig::log_step_limit = parse_log_steps_from_argv(argc, argv);
    SimLogConfig::current_step = 0;
    SimLogConfig::step_scope_enabled = false;

    if (sim_debug_runtime_enabled()) {
        LogRedirector log;
        if (log.start(elf)) {
            simulator(elf, max_steps);
            log.stop();
        } else {
            std::cerr << "警告: 无法打开日志文件，仅输出到终端。\n";
            simulator(elf, max_steps);
        }
    } else {
        simulator(elf, max_steps);
    }
    return 0;
}

#endif
