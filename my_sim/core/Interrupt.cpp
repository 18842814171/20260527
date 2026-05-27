// Interrupt.cpp
#include "Interrupt.hpp"
#include "CPU.hpp"
#include "device/UART.hpp"
#include "utils/utils.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

namespace {
UART* g_interrupt_trace_uart = nullptr;
}

void set_interrupt_trace_uart(UART* uart) {
    g_interrupt_trace_uart = uart;
}

void clear_interrupt_trace_uart() {
    g_interrupt_trace_uart = nullptr;
}

void interrupt_trace_log(const std::ostringstream& line) {
    if (g_interrupt_trace_uart) {
        g_interrupt_trace_uart->host_trace_line(line.str().c_str());
    }
}

#define INTERRUPT_LOG(msg)                                                         \
    do {                                                                           \
        std::ostringstream _irq_os_;                                               \
        _irq_os_ << msg;                                                           \
        if (sim_debug_runtime_enabled()) {                                         \
            std::cout << "[Interrupt] " << _irq_os_.str() << std::endl;             \
        }                                                                          \
        if (sim_trap_uart_trace_enabled() && g_interrupt_trace_uart) {             \
            g_interrupt_trace_uart->host_trace_line(                               \
                (std::string("[Interrupt] ") + _irq_os_.str()).c_str());           \
        }                                                                          \
    } while (0)

uint32_t MachineCSR::interrupt_cause(InterruptType type) {
    switch (type) {
        case InterruptType::SOFTWARE: return 3;
        case InterruptType::TIMER: return 7;
        case InterruptType::EXTERNAL: return 11;
        default: return 0;
    }
}

uint32_t MachineCSR::mip_bit(InterruptType type) {
    switch (type) {
        case InterruptType::SOFTWARE: return MIP_MSIP;
        case InterruptType::TIMER: return MIP_MTIP;
        case InterruptType::EXTERNAL: return MIP_MEIP;
        default: return 0;
    }
}

uint32_t MachineCSR::mie_bit(InterruptType type) {
    return mip_bit(type);
}

uint32_t MachineCSR::trap_vector(bool is_interrupt, uint32_t cause) const {
    const uint32_t base = mtvec & ~0x3u;
    if (!is_interrupt) {
        return base;
    }
    // mtvec[1:0]==1: vectored (IRQ -> BASE + 4*cause); ==0: direct (all -> BASE).
    if ((mtvec & 0x3u) == 1u) {
        return base + (cause * 4u);
    }
    return base;
}

uint32_t MachineCSR::enter_trap(bool is_interrupt, uint32_t cause, uint32_t epc) {
    trapping_ = true;
    mepc = epc;
    mcause = is_interrupt ? (0x80000000u | cause) : cause;
    mtval = 0;

    const uint32_t prev_mie = mstatus & MSTATUS_MIE;
    mstatus &= ~MSTATUS_MIE;
    mstatus &= ~MSTATUS_MPIE;
    mstatus |= (prev_mie ? MSTATUS_MPIE : 0);
    mstatus &= ~MSTATUS_MPP;
    mstatus |= (static_cast<uint32_t>(PrivilegeMode::MACHINE) << 11);

    const uint32_t trap_pc = trap_vector(is_interrupt, cause);
    INTERRUPT_LOG("TRAP: enter is_interrupt=" << is_interrupt
          << " cause=0x" << std::hex << cause
          << " epc=0x" << mepc << " vec=0x" << trap_pc << std::dec);
    return trap_pc;
}

uint32_t MachineCSR::mret() {
    if (!trapping_) {
        return mepc;
    }
    trapping_ = false;

    const uint32_t mpie = mstatus & MSTATUS_MPIE;
    mstatus &= ~MSTATUS_MIE;
    mstatus |= mpie ? MSTATUS_MIE : 0;
    mstatus |= MSTATUS_MPIE;
    mstatus &= ~MSTATUS_MPP;

    INTERRUPT_LOG("TRAP: mret to epc=0x" << std::hex << mepc << std::dec);
    return mepc;
}

InterruptController::InterruptController(MachineCSR* csr) : csr_(csr) {}

uint32_t InterruptController::effective_mip() const {
    if (!csr_) {
        return hw_mip_;
    }
    return (csr_->mip_sw & MachineCSR::MIP_MSIP) | hw_mip_;
}

void InterruptController::request_interrupt(InterruptType type, uint32_t source) {
    if (!csr_) {
        return;
    }

    const uint32_t bit = MachineCSR::mip_bit(type);
    if (bit == 0) {
        return;
    }

    /* Always enqueue: coalescing only hw_mip left MEIP set with an empty queue
     * (guest UART spin never woke up — io_line stuck after first key). */
    if (type == InterruptType::EXTERNAL) {
        ext_request_queued_ = true;
    } else if (type == InterruptType::TIMER) {
        timer_request_queued_ = true;
    }

    pending_interrupts.emplace(type, source, cycle_count);
    hw_mip_ |= bit;

    const char* type_name = "?";
    switch (type) {
        case InterruptType::TIMER: type_name = "TIMER"; break;
        case InterruptType::EXTERNAL: type_name = "EXTERNAL"; break;
        case InterruptType::SOFTWARE: type_name = "SOFTWARE"; break;
        default: break;
    }
    INTERRUPT_LOG("IRQ pending: " << type_name
          << " source=" << source << " @ cycle " << cycle_count);
}

bool InterruptController::has_pending_interrupt() const {
    if (!csr_ || !csr_->global_interrupt_enabled()) {
        return false;
    }
    const uint32_t mip = effective_mip();
    const uint32_t enabled_pending = csr_->mie & mip & MachineCSR::IRQ_MASK;
    return enabled_pending != 0 && !pending_interrupts.empty();
}

InterruptRequest InterruptController::get_pending_interrupt() {
    if (!has_pending_interrupt()) {
        return InterruptRequest(InterruptType::SOFTWARE, 0, cycle_count);
    }

    static const InterruptType kPriority[] = {
        InterruptType::EXTERNAL,
        InterruptType::SOFTWARE,
        InterruptType::TIMER,
    };

    const uint32_t mip = effective_mip();
    for (InterruptType preferred : kPriority) {
        const uint32_t bit = MachineCSR::mip_bit(preferred);
        if ((csr_->mie & bit) == 0 || (mip & bit) == 0) {
            continue;
        }

        std::queue<InterruptRequest> remaining;
        InterruptRequest selected(InterruptType::SOFTWARE, 0, cycle_count);
        bool found = false;
        while (!pending_interrupts.empty()) {
            InterruptRequest irq = pending_interrupts.front();
            pending_interrupts.pop();
            if (!found && irq.type == preferred) {
                selected = irq;
                found = true;
            } else {
                remaining.push(irq);
            }
        }
        pending_interrupts = std::move(remaining);

        if (found) {
            if (selected.type == InterruptType::EXTERNAL) {
                ext_request_queued_ = false;
            } else if (selected.type == InterruptType::TIMER) {
                timer_request_queued_ = false;
            }
            hw_mip_ &= ~MachineCSR::mip_bit(selected.type);
            return selected;
        }
    }

    InterruptRequest irq = pending_interrupts.front();
    pending_interrupts.pop();
    if (irq.type == InterruptType::EXTERNAL) {
        ext_request_queued_ = false;
    } else if (irq.type == InterruptType::TIMER) {
        timer_request_queued_ = false;
    }
    hw_mip_ &= ~MachineCSR::mip_bit(irq.type);
    return irq;
}

void InterruptController::clear_interrupt() {
    hw_mip_ = 0;
    ext_request_queued_ = false;
    timer_request_queued_ = false;
    while (!pending_interrupts.empty()) {
        pending_interrupts.pop();
    }
}

uint32_t InterruptController::read_csr(uint32_t addr) const {
    if (!csr_) {
        return 0;
    }
    switch (addr) {
        case MSTATUS_ADDR: return csr_->mstatus;
        case MIE_ADDR: return csr_->mie;
        case MTVEC_ADDR: return csr_->mtvec;
        case MSCRATCH_ADDR: return csr_->mscratch;
        case MEPC_ADDR: return csr_->mepc;
        case MCAUSE_ADDR: return csr_->mcause;
        case MTVAL_ADDR: return csr_->mtval;
        case MIP_ADDR: return effective_mip();
        default: return 0;
    }
}

void InterruptController::write_csr(uint32_t addr, uint32_t value) {
    if (!csr_) {
        return;
    }
    switch (addr) {
        case MSTATUS_ADDR:
            csr_->mstatus = value;
            break;
        case MIE_ADDR:
            csr_->mie = value;
            break;
        case MTVEC_ADDR:
            csr_->mtvec = value;
            break;
        case MSCRATCH_ADDR:
            csr_->mscratch = value;
            break;
        case MEPC_ADDR:
            csr_->mepc = value;
            break;
        case MCAUSE_ADDR:
            csr_->mcause = value;
            break;
        case MTVAL_ADDR:
            csr_->mtval = value;
            break;
        case MIP_ADDR:
            csr_->mip_sw = (value & MachineCSR::MIP_MSIP) | (csr_->mip_sw & ~MachineCSR::MIP_MSIP);
            break;
        default:
            break;
    }
}

void InterruptController::tick() {
    cycle_count++;
}

TrapHandler::TrapHandler(MachineCSR* csr) : csr_(csr) {}

void TrapHandler::enter_trap(bool is_interrupt, uint32_t cause, uint32_t epc, uint32_t& out_pc) {
    if (!csr_) {
        out_pc = epc;
        return;
    }
    out_pc = csr_->enter_trap(is_interrupt, cause, epc);
}

void TrapHandler::exit_trap(uint32_t& out_pc) {
    if (!csr_) {
        out_pc = 0;
        return;
    }
    out_pc = csr_->mret();
}

uint32_t TrapHandler::get_epc() const {
    return csr_ ? csr_->mepc : 0;
}

uint32_t TrapHandler::get_cause() const {
    return csr_ ? csr_->mcause : 0;
}

bool TrapHandler::is_trapping() const {
    return csr_ && csr_->is_trapping();
}

uint32_t TrapHandler::get_mstatus() const {
    return csr_ ? csr_->mstatus : 0;
}

uint32_t TrapHandler::get_mtval() const {
    return csr_ ? csr_->mtval : 0;
}

uint32_t TrapHandler::get_mtvec() const {
    return csr_ ? csr_->mtvec : 0;
}

void TrapHandler::set_mtvec(uint32_t addr) {
    if (csr_) {
        csr_->mtvec = addr;
    }
}

void wire_cpu_interrupts(CPU& cpu, MachineCSR& csr,
                         InterruptController& int_ctrl, TrapHandler& trap) {
    int_ctrl.set_csr(&csr);
    trap.set_csr(&csr);
    cpu.set_machine_csr(&csr);
    cpu.set_interrupt_controller(&int_ctrl);
    cpu.set_trap_handler(&trap);
    cpu.enable_interrupts();
}

void init_default_machine_csrs(MachineCSR& csr) {
    csr.mstatus = 0;
    csr.mie = 0;
    csr.mtvec = 0;
}
