#ifndef INTERRUPT_HPP
#define INTERRUPT_HPP

#include <cstdint>
#include <queue>

class CPU;

enum class InterruptType : uint8_t {
    TIMER = 0,
    EXTERNAL = 1,
    SOFTWARE = 2,
    SYSCALL = 3,
};

enum class PrivilegeMode : uint8_t {
    USER = 0,
    SUPERVISOR = 1,
    MACHINE = 3,
};

struct InterruptRequest {
    InterruptType type;
    uint32_t source;
    uint32_t timestamp;
    bool pending;

    InterruptRequest(InterruptType t, uint32_t src, uint32_t ts)
        : type(t), source(src), timestamp(ts), pending(true) {}
};

/** Unified M-mode CSR state and trap enter/return (mret). */
class MachineCSR {
public:
    static constexpr uint32_t MSTATUS_MIE  = 1u << 3;
    static constexpr uint32_t MSTATUS_MPIE = 1u << 7;
    static constexpr uint32_t MSTATUS_MPP  = 3u << 11;

    static constexpr uint32_t MIE_MSIE = 1u << 3;
    static constexpr uint32_t MIE_MTIE = 1u << 7;
    static constexpr uint32_t MIE_MEIE = 1u << 11;

    static constexpr uint32_t MIP_MSIP = 1u << 3;
    static constexpr uint32_t MIP_MTIP = 1u << 7;
    static constexpr uint32_t MIP_MEIP = 1u << 11;

    static constexpr uint32_t IRQ_MASK = MIP_MSIP | MIP_MTIP | MIP_MEIP;

    static uint32_t interrupt_cause(InterruptType type);
    static uint32_t mip_bit(InterruptType type);
    static uint32_t mie_bit(InterruptType type);

    uint32_t mstatus = 0;
    uint32_t mie = 0;
    uint32_t mip_sw = 0;
    uint32_t mtvec = 0x100;
    uint32_t mscratch = 0;
    uint32_t mepc = 0;
    uint32_t mcause = 0;
    uint32_t mtval = 0;

    bool global_interrupt_enabled() const { return (mstatus & MSTATUS_MIE) != 0; }
    bool is_trapping() const { return trapping_; }

    uint32_t trap_vector(bool is_interrupt, uint32_t cause) const;
    uint32_t enter_trap(bool is_interrupt, uint32_t cause, uint32_t epc);
    uint32_t mret();

private:
    bool trapping_ = false;
};

class InterruptController {
public:
    static constexpr uint32_t MTVEC_BASE = 0x100;
    static constexpr uint32_t MSTATUS_ADDR = 0x300;
    static constexpr uint32_t MIE_ADDR = 0x304;
    static constexpr uint32_t MTVEC_ADDR = 0x305;
    static constexpr uint32_t MSCRATCH_ADDR = 0x340;
    static constexpr uint32_t MEPC_ADDR = 0x341;
    static constexpr uint32_t MCAUSE_ADDR = 0x342;
    static constexpr uint32_t MTVAL_ADDR = 0x343;
    static constexpr uint32_t MIP_ADDR = 0x344;

    explicit InterruptController(MachineCSR* csr = nullptr);

    void set_csr(MachineCSR* csr) { csr_ = csr; }
    MachineCSR* get_csr() { return csr_; }
    const MachineCSR* get_csr() const { return csr_; }

    void request_interrupt(InterruptType type, uint32_t source = 0);
    bool has_pending_interrupt() const;
    InterruptRequest get_pending_interrupt();
    void clear_interrupt();

    uint32_t effective_mip() const;
    uint32_t read_csr(uint32_t addr) const;
    void write_csr(uint32_t addr, uint32_t value);

    void tick();

private:
    MachineCSR* csr_ = nullptr;
    bool ext_request_queued_ = false;
    bool timer_request_queued_ = false;
    std::queue<InterruptRequest> pending_interrupts;
    uint32_t cycle_count = 0;
    uint32_t hw_mip_ = 0;
};

class TrapHandler {
public:
    explicit TrapHandler(MachineCSR* csr = nullptr);

    void set_csr(MachineCSR* csr) { csr_ = csr; }

    void enter_trap(bool is_interrupt, uint32_t cause, uint32_t epc, uint32_t& out_pc);
    void exit_trap(uint32_t& out_pc);

    uint32_t get_epc() const;
    uint32_t get_cause() const;
    bool is_trapping() const;

    uint32_t get_mstatus() const;
    uint32_t get_mtval() const;
    uint32_t get_mtvec() const;
    void set_mtvec(uint32_t addr);

private:
    MachineCSR* csr_ = nullptr;
};

void wire_cpu_interrupts(CPU& cpu, MachineCSR& csr,
                         InterruptController& int_ctrl, TrapHandler& trap);
void init_default_machine_csrs(MachineCSR& csr);

class UART;
/** Host UART used to mirror [Interrupt] lines to stdout (guest TX path). */
void set_interrupt_trace_uart(UART* uart);
void clear_interrupt_trace_uart();

#endif
