//simulator.cpp
#include "simulator.hpp"
#include "utils/utils.hpp"
#include "Instmngr.hpp"
#include "CPU.hpp"
#include "device/Device.hpp"
#include "device/Bus.hpp"
#include "device/Timer.hpp"
#include "device/UART.hpp"
#include "device/Display.hpp"
#include "Decoder.hpp"
#include "Loader.hpp"
#include "Interrupt.hpp"
#include "SimulatorAPI.hpp"
#include "inst/arithm.hpp"
#include "inst/load_store.hpp"
#include "inst/auipc.hpp"
#include "inst/branch.hpp"
#include "inst/jump.hpp"
#include "inst/system.hpp"
#include "inst/lui.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>

extern void register_all_instructions(InstManager *im);
namespace {

/** Device / ISA statistics go to LOG (→ log file when LogRedirector is active), not the terminal. */
static void log_device_status(const char* title, Memory& mem, const CPU& cpu, const Timer& timer,
                             const UART& uart) {
    LOG("");
    LOG(std::string("=== ") + title + " ===");
    {
        std::ostringstream oss;
        oss << "主存: 容量 " << Memory::capacity_bytes() << " bytes, 基址 0x"
            << std::hex << Memory::base_address() << std::dec;
        LOG(oss.str());
    }
    const uint32_t hw = mem.high_water_byte_address();
    if (hw == 0) {
        LOG("      主存区内尚无写入记录(或仅 MMIO)");
    } else {
        std::ostringstream oss;
        oss << "      已写入最高字节地址 0x" << std::hex << hw << std::dec
            << " (相对基址 +" << (hw - Memory::base_address()) << " bytes)";
        LOG(oss.str());
    }
    LOG("Timer MTIME: " + std::to_string(timer.get_mtime()));
    LOG("UART: TX 字符(送主机) " + std::to_string(uart.tx_character_count()) +
        ", RX 字符(程序已读走) " + std::to_string(uart.rx_character_count()) +
        ", 当前 RX FIFO " + std::to_string(uart.rx_count()));
    std::ostringstream stats;
    cpu.dump_instruction_statistics(stats);
    LOG(stats.str());
}

static void log_register_dump(const CPU& cpu) {
    std::ostringstream oss;
    cpu.dump_registers(oss);
    LOG(oss.str());
}
} // namespace

void simulator(std::string infile, size_t max_steps) {
    InstManager im;
    register_all_instructions(&im);

    Memory mem;
    CPU cpu(mem, im);

    static MachineCSR machine_csr;
    static InterruptController int_ctrl(&machine_csr);
    static TrapHandler trap(&machine_csr);
    wire_cpu_interrupts(cpu, machine_csr, int_ctrl, trap);
    init_default_machine_csrs(machine_csr);

    uint32_t entry_point = load_elf(infile, mem);
    uint32_t main_addr = get_symbol(infile, "main");
    uint32_t gp_val = get_symbol(infile, "__global_pointer$");

    LOG("Loading ELF: " + infile);
    LOG("Program entry point: 0x" + HEX(entry_point));
    LOG("Main address: 0x" + HEX(main_addr));
    if (gp_val) {
        LOG("GP initialized to 0x" + HEX(gp_val));
    }

    auto args = setup_args_for_elf(infile, 0x20000, mem);

    cpu.set_pc(entry_point);
    cpu.reg[0] = 0;
    cpu.reg[1] = 0xFFFFFFFF;
    cpu.reg[2] = args.sp;
    cpu.reg[3] = gp_val;
    cpu.reg[4] = 0;
    cpu.reg[10] = args.argc;
    cpu.reg[11] = args.argv_addr;
    cpu.reg[12] = 0;
    cpu.reg[17] = 0;

    Bus bus;
    bus.attach_memory(&mem);

    Timer timer;
    bus.attach_device(0x02004000, &timer);

    UART uart;
    bus.attach_device(0x10000000, &uart);

    DisplayDevice display(mem);
    bus.attach_device(0x10001000, &display);

    cpu.attach_bus(&bus);
    cpu.attach_uart(&uart);

    if (sim_trap_uart_trace_enabled()) {
        set_interrupt_trace_uart(&uart);
    }

    const char* uart_inject = std::getenv("SIM_INJECT_UART");
    const bool uart_inject_mode = uart_inject && uart_inject[0] != '\0';
    if (uart_inject_mode) {
        uart.put_char(static_cast<uint8_t>(uart_inject[0]));
    }

    uart.reset_transfer_counters();
    cpu.reset_instruction_statistics();

    if (sim_debug_runtime_enabled()) {
        log_device_status("运行前（设备/内存/统计）", mem, cpu, timer, uart);
        if (uart_inject_mode) {
            LOG("[UART: SIM_INJECT_UART — host stdin thread disabled for deterministic RX]");
        } else {
            LOG("[UART: 主机键盘 → MMIO RX；Ctrl+C 结束 stdin 线程]");
        }
        if (max_steps > 0) {
            LOG("[模拟器] 本次步数上限(请求) " + std::to_string(max_steps) +
                "，CPU::kHardAbsoluteRunStepLimit=" + std::to_string(CPU::kHardAbsoluteRunStepLimit));
        } else {
            LOG("[模拟器] 未指定步数上限，使用默认 CPU::kDefaultRunStepLimit=" +
                std::to_string(CPU::kDefaultRunStepLimit) + " 周期");
        }
        std::cerr << "[sim] Guest UART TX on stdout below (statistics → log/ file).\n";
        if (sim_trap_uart_trace_enabled()) {
            std::cerr << "[sim] [Interrupt] TRAP lines also on stdout (SIM_TRAP_UART / --trap-uart=1).\n";
        }
        std::cerr << "\n";
    }

    if (!uart_inject_mode) {
        uart.start_console_input(STDIN_FILENO);
    }

    LOG("\n========== Running ==========");
    cpu.run(max_steps);

    if (!uart_inject_mode) {
        uart.stop_console_input();
    }

    if (sim_debug_runtime_enabled()) {
        log_device_status("运行后（设备/内存/统计）", mem, cpu, timer, uart);
        std::cerr << "\n[sim] run finished: exit_code=" << cpu.exit_code
                  << " steps=" << cpu.step_count
                  << " halted=" << (cpu.halt ? 1 : 0) << " (see log/ for details)\n";
    } else {
        std::cerr << "[模拟器] 运行结束: halted=" << (cpu.halt ? 1 : 0)
                  << " steps=" << cpu.step_count
                  << " exit_code=" << cpu.exit_code
                  << " pc=0x" << std::hex << cpu.pc << std::dec << "\n";
    }

    LOG("\nRan " + std::to_string(cpu.step_count) + " steps.");
    LOG("PC: 0x" + HEX(cpu.pc));
    LOG("Exit code: " + std::to_string(cpu.exit_code));

    LOG("\n=== Final Register State ===");
    if (sim_debug_runtime_enabled())
        log_register_dump(cpu);


}
