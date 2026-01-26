// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#ifndef AXCORE_HPP_INCLUDED
#define AXCORE_HPP_INCLUDED

#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <span>

#ifndef NDEBUG
    #include <iostream>
    #include <iomanip>
#endif

#include "opcode.hpp"
#include "panic.hpp"
#include "memory.hpp"

class AxCore
{
public:
    using Register = uint32_t;
    static constexpr Register REG_LR = 31;
    static constexpr Register REG_ACC = 56;
    static constexpr Register REG_BA1 = 57;
    static constexpr Register REG_BA2 = 58;
    static constexpr Register REG_BF1 = 59;
    static constexpr Register REG_BF2 = 60;
    static constexpr Register REG_BL1 = 61;
    static constexpr Register REG_BL2 = 62;
    static constexpr Register REG_ZERO = 63;

    static constexpr uint64_t ICACHE_SIZE = 0x10000 / 1024; //(64 KiB , 4-way)
    static constexpr uint64_t DCACHE_SIZE = 0x8000 / 128;   //(32 KiB , 4-way)

    static constexpr uint32_t IO_VOID = 0;
    static constexpr uint32_t IO_READ = 1;
    static constexpr uint32_t IO_WRITE = 2;

    static constexpr uint32_t Z_MASK = 0x01;
    static constexpr uint32_t C_MASK = 0x02;
    static constexpr uint32_t N_MASK = 0x04;
    static constexpr uint32_t O_MASK = 0x08;
    static constexpr uint32_t U_MASK = 0x10;

    static constexpr uint64_t MAX_CORES = 64;
    static constexpr uint64_t IREG_COUNT = 64;
    static constexpr uint64_t VREG_COUNT = 64;
    static constexpr uint64_t SPM_SIZE = 0x4000;

    struct RegisterSet
    {
        uint32_t lr{}; // link-register
        uint32_t br{}; // branch-register
        uint32_t lc{}; // loop counter
        uint32_t fr{}; // flag register
        uint32_t pc{}; // program-counter
        uint32_t ir{}; // interrupt-register
        uint32_t cc{}; // cycle counter
        uint32_t ic{}; // instruction counter

        // General purpose integer regs
        std::array<uint64_t, IREG_COUNT> gpi{};
        // General purpose fp regs, use accessor functions for typed access (float ect)
        std::array<uint64_t, VREG_COUNT> gpf{};
        // MDU registers (Q, QR, PL, PH)
        std::array<uint64_t, 4> mdu{};
        // EFU register
        uint64_t efu_q{};
    };

    AxCore(AxMemory& memory);
    ~AxCore() = default;
    AxCore(const AxCore&) = delete;
    AxCore& operator=(const AxCore&) = delete;
    AxCore(AxCore&&) noexcept = delete;
    AxCore& operator=(AxCore&&) noexcept = delete;

    // Execute opcode1 and, if possible, opcode2. Returns the number of opcodes run (1 or 2)
    // Used internally in cycle(), may be used in tests.
    uint32_t execute(AxOpcode first, AxOpcode second);

    // Emulate a whole cycle. Read next instructions from current PC and update it.
    void cycle()
    {
        const auto real_pc = m_regs.pc & 0x7FFFFFFF;
        const auto opcode1 = m_wram_begin[real_pc];
        const auto opcode2 = m_wram_begin[real_pc + 1u];

#ifndef NDEBUG
        // TODO: This code has to be moved somewhere else
        static int noop_counter = 0;
        if(AxOpcode{opcode1}.operation() == 0)
        {
            if(++noop_counter > 16) // more than 16 noops in row is obviously a broken jump
            {
                ax_panic("Suspitious code.");
            }
        }
        else
        {
            noop_counter = 0;
        }

        const auto pc_addr = real_pc * 4ull;
        bool skip = false;

        auto closest = std::lower_bound(std::begin(m_symbols), std::end(m_symbols), pc_addr, [](auto&& left, auto&& right)
        {
            return left.address < right;
        });

        if(closest != std::end(m_symbols) && closest != std::begin(m_symbols))
        {
            if(closest->address != pc_addr)
            {
                closest = std::prev(closest);
            }

            if(closest->name.find("memset") == std::string::npos)
            {
                if(closest->name.find("_ZN19__llvm_libc_20_1_2_") != std::string::npos)
                {
                    std::cout << closest->name.substr(25, 64);
                }
                else
                {
                    std::cout << closest->name.substr(0, 64);
                }

                std::cout << "+" << (pc_addr - closest->address) << " | ";
            }
            else
            {
                skip = true;
            }
        }
        else // print raw PC
        {
            std::cout << std::hex << std::showbase << std::setw(12) << pc_addr << " | ";
        }

        if(!skip)
        {
            if(AxOpcode{opcode1}.is_bundle())
            {
                auto [first, second] = AxOpcode::analyze(opcode1, opcode2);
                std::cout << first.to_string(nullptr) << " ; " << second.to_string(nullptr) << std::endl;
            }
            else
            {
                auto first = AxOpcode::analyze(opcode1, {}).first;
                std::cout << first.to_string(nullptr) << std::endl;
            }
        }
#endif

        const auto count = execute(opcode1, opcode2);

        m_regs.cc += 1;
        m_regs.ic += count;
        m_regs.pc += count;
    }

    // Emulate a syscalls if last executed bundle included a syscall instruction.
    // Returns true if a syscall was executed.
    // This may be used to immediately react to state change caused by the syscall!
    template<typename HandlerT, typename... Args>
    bool syscall(HandlerT&& handler, Args&&... args)
    {
        // Shortcut, this function is inline for this
        if(m_syscall == 0) [[likely]] // 99.99999% of the time true (very serious study)
        {
            return false;
        }

        std::invoke(std::forward<HandlerT>(handler), std::forward<Args>(args)...);

        m_syscall = 0;
        return true;
    }

    AxMemory& memory() noexcept
    {
        return *m_memory;
    }

    const AxMemory& memory() const noexcept
    {
        return *m_memory;
    }

    RegisterSet& registers() noexcept
    {
        return m_regs;
    }

    const RegisterSet& registers() const noexcept
    {
        return m_regs;
    }

    uint8_t* smp_data() noexcept
    {
        return m_spm.data();
    }

    const uint8_t* smp_data() const noexcept
    {
        return m_spm.data();
    }

    int error() const noexcept
    {
        return m_error;
    }

    struct Symbol
    {
        uint64_t address{};
        uint64_t size{};
        std::string name{};
    };

    void set_symbols(std::vector<Symbol> symbols) noexcept
    {
        m_symbols = std::move(symbols);
    }

    std::span<const Symbol> symbols() const noexcept
    {
        return m_symbols;
    }

    struct Breakpoint
    {
        uint64_t address{};
        bool enabled{true};
        bool single_shot{false};
    };

    // if breakpoint at address already exists, it will be enabled or disabled based on `enabled`.
    // if breakpoint at address already exists, it will not become single shot even if `single_shot` is true.
    void add_breakpoint(Breakpoint bp);

    void remove_breakpoint(uint64_t address);
    void remove_breakpoint(const Breakpoint* bp)
    {
        const auto index = std::distance(std::as_const(m_breakpoints).data(), bp);
        m_breakpoints.erase(m_breakpoints.begin() + index);
    }

    Breakpoint* breakpoint_at(uint64_t address);
    const Breakpoint* breakpoint_at(uint64_t address) const;

    // Return a breakpoint if current PC is on it.
    // Note that it is up to caller to check if breakpoint is enabled!
    const Breakpoint* hit_breakpoint() const
    {
        // Shortcut, this function is inline for this (same as syscall)
        if(m_breakpoints.empty()) [[likely]]
        {
            return nullptr;
        }

        return breakpoint_at(pc_to_wram(m_regs.pc));
    }

    std::span<const Breakpoint> breakpoints() const noexcept
    {
        return m_breakpoints;
    }

    static uint64_t pc_to_wram(uint64_t pc) noexcept
    {
        return AxMemory::WRAM_BEGIN + (pc & 0x7FFFFFFF) * 4ull;
    }

    static uint64_t pc_from_wram(uint64_t address) noexcept
    {
        return (address - AxMemory::WRAM_BEGIN) / 4ull;
    }

private:
    void do_store(uint64_t src, uint64_t addr, uint32_t size);
    uint64_t do_load(uint64_t addr, uint32_t size);

    void io_read(uint64_t offset, void* reg);
    void io_write(uint64_t offset, void* reg);

    void execute_unit(AxOpcode opcode, uint32_t slot, uint64_t imm24);

    /*
    UNIT ID |    UNIT NAME
            | INST 1 | INST 2
       0    |  ALU1  |  ALU2
       1    |  ALU1  |  ALU2
       2    |  LSU1  |  LSU2
       3    |  FPU1  |  FPU2
       4    |  /     |   /
       5    |  EFU   |   CU
       6    |  MDU   |   VU
       7    |  BRU   |   /
    */
    void execute_alu(AxOpcode op, uint32_t slot, uint64_t imm24);
    void execute_mdu(AxOpcode op, uint64_t imm24);
    void execute_lsu(AxOpcode op, uint32_t slot, uint64_t imm24);
    void execute_bru(AxOpcode op, uint64_t imm24);
    void execute_fpu(AxOpcode op, uint32_t slot, uint64_t imm24);
    void execute_efu(AxOpcode op, uint64_t imm24);
    void execute_cu(AxOpcode op, uint64_t imm24);
    void execute_vu(AxOpcode op, uint64_t imm24);

    std::array<uint8_t, SPM_SIZE> m_spm{};
    RegisterSet m_regs{};
    AxMemory* m_memory{};
    const uint32_t* m_wram_begin{};

    int m_error = 0;
    uint32_t m_cycle = 0;
    uint32_t m_instruction = 0;
    uint32_t m_syscall = 0;

    std::vector<Breakpoint> m_breakpoints{};
    std::vector<Symbol> m_symbols{};
};

#endif
