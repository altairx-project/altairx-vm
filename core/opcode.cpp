// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "opcode.hpp"

#include <fmt/format.h>
#include <variant>
#include <iterator>

#include "utilities.hpp"
#include "panic.hpp"
#include "core.hpp"

namespace AxOpcodeArg
{

std::string format_as(Reg r)
{
    const auto reg = r.id;
    if(reg == 0)
    {
        return "sp";
    }
    else if(1 <= reg && reg <= 8)
    {
        return fmt::format("a{}", reg - 1);
    }
    else if(9 <= reg && reg <= 19)
    {
        return fmt::format("s{}", reg - 9);
    }
    else if(20 <= reg && reg <= 30)
    {
        return fmt::format("t{}", reg - 20);
    }
    else if(reg == 31)
    {
        return "lr";
    }
    else if(32 <= reg && reg <= 55)
    {
        return fmt::format("n{}", reg - 32);
    }
    else if(reg == 56)
    {
        return "acc";
    }
    else if(reg == 63)
    {
        return "zero";
    }

    return fmt::format("r{}", reg);
}

std::string format_as(FReg r)
{
    return fmt::format("v{}", r.id);
}

std::string format_as(MDUReg r)
{
    switch(r.id)
    {
    case 0:
        return "Q";
    case 1:
        return "QR";
    case 2:
        return "PL";
    case 3:
        return "PH";
    default:
        return "?";
    }
}

std::string format_as(UImm imm)
{
    if(imm.hexa_decimal)
    {
        return fmt::format("{0:#x}", imm.value);
    }

    return fmt::format("{}", imm.value);
}

std::string format_as(SImm imm)
{
    return fmt::format("{}", imm.value);
}

std::string format_as(AbsoluteLabel label)
{
    return fmt::format("{}", label.value);
}

std::string format_as(RelativeLabel label)
{
    return fmt::format("{}", label.value);
}

std::string format_as(Size size)
{
    switch(size.value)
    {
    case 0:
        return ".b";
    case 1:
        return ".w";
    case 2:
        return ".d";
    case 3:
        return ".q";
    default:
        return ".?";
    }
}

std::string format_as(FSize size)
{
    switch(size.value)
    {
    case 0:
        return ".s";
    case 1:
        return ".d";
    default:
        return ".?";
    }
}

std::string format_as(ShiftedReg shift)
{
    if(shift.shift > 0)
    {
        return fmt::format("{} << {}", Reg(shift.reg), shift.shift);
    }

    return fmt::format("{}", Reg(shift.reg));
}

std::string format_as(Operand op)
{
    const auto visitors = ax_overloads{
        [](std::monostate) -> std::string
    {
        return "???";
    },
        [](auto&& alternative) -> std::string
    {
        return fmt::format("{}", alternative);
    }};

    return std::visit(visitors, op.value);
}

AxOpcodeInfo analyze_alu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto output = [op]() -> Reg
    {
        return Reg(op.reg_a());
    };

    const auto left = [op]() -> Reg
    {
        return Reg(op.reg_b());
    };

    const auto right = [op, imm24]() -> Operand
    {
        if(!op.alu_has_imm())
        {
            return Operand(ShiftedReg(op.reg_c(), op.alu_shift()));
        }

        const uint64_t tmp = sext_bitsize(op.alu_imm9(), 9);
        // apply imm24
        return Operand(SImm(static_cast<int64_t>(tmp ^ (imm24 << 8))));
    };

    const auto size = [op]() -> Size
    {
        return Size(op.size());
    };

    // Most opcodes share the same format!
    const auto format_default = [&](auto&& name)
    {
        return AxOpcodeInfo(name, size(), output(), left(), right());
    };

    switch(op.operation())
    {
    case AX_EXE_ALU_MOVEIX: // no-op
        return issecond ? AxOpcodeInfo("moveix") : AxOpcodeInfo("nop");
    case AX_EXE_ALU_MOVEI:
        return AxOpcodeInfo("movei", output(), SImm(static_cast<int64_t>(sext_bitsize(op.alu_move_imm(), 18) ^ (imm24 << 17))));
    case AX_EXE_ALU_EXT:
        return AxOpcodeInfo("ext", output(), left(), UImm(op.ext_ins_imm1()), UImm(op.ext_ins_imm2()));
    case AX_EXE_ALU_INS:
        return AxOpcodeInfo("ins", output(), left(), UImm(op.ext_ins_imm1()), UImm(op.ext_ins_imm2()));

    case AX_EXE_ALU_MAX:
        return format_default("max");
    case AX_EXE_ALU_UMAX:
        return format_default("umax");
    case AX_EXE_ALU_MIN:
        return format_default("min");
    case AX_EXE_ALU_UMIN:
        return format_default("umin");

    case AX_EXE_ALU_ADDS:
        return format_default("adds");
    case AX_EXE_ALU_SUBS:
        return format_default("subs");

    case AX_EXE_ALU_CMP:
        return AxOpcodeInfo("cmp", size(), left(), right());
    case AX_EXE_ALU_BIT:
        return AxOpcodeInfo("bit", size(), left(), right());
    case AX_EXE_ALU_TEST:
        return AxOpcodeInfo("test", size(), left(), right());
    case AX_EXE_ALU_TESTFR:
        return AxOpcodeInfo("testfr", size(), right());

    case AX_EXE_ALU_ADD:
        return format_default("add");
    case AX_EXE_ALU_SUB:
        return format_default("sub");
    case AX_EXE_ALU_XOR:
        return format_default("xor");
    case AX_EXE_ALU_OR:
        return format_default("or");

    case AX_EXE_ALU_AND:
        return format_default("and");
    case AX_EXE_ALU_LSL:
        return format_default("lsl");
    case AX_EXE_ALU_ASR:
        return format_default("asr");
    case AX_EXE_ALU_LSR:
        return format_default("lsr");

    case AX_EXE_ALU_SE:
        return format_default("se");
    case AX_EXE_ALU_SEN:
        return format_default("sen");
    case AX_EXE_ALU_SLTS:
        return format_default("slts");
    case AX_EXE_ALU_SLTU:
        return format_default("sltu");

    case AX_EXE_ALU_SAND:
        return format_default("sand");
    case AX_EXE_ALU_SBIT:
        return format_default("sbit");
    case AX_EXE_ALU_CMOVEN:
        return format_default("cmoven");
    case AX_EXE_ALU_CMOVE:
        return format_default("cmove");
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_mdu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto output = [op]() -> Reg
    {
        return Reg(op.reg_a());
    };

    const auto left = [op]() -> Reg
    {
        return Reg(op.reg_b());
    };

    const auto right = [op, imm24]() -> Operand
    {
        if(!op.alu_has_imm())
        {
            return Operand(ShiftedReg(op.reg_c(), op.alu_shift()));
        }

        const uint64_t tmp = sext_bitsize(op.alu_imm9(), 9);
        // apply imm24
        return Operand(SImm(static_cast<int64_t>(tmp ^ (imm24 << 8))));
    };

    const auto size = [op]() -> Size
    {
        return Size(op.size());
    };

    switch(op.operation())
    {
    case AX_EXE_MDU_DIV:
        return AxOpcodeInfo("div", size(), left(), right());
    case AX_EXE_MDU_DIVU:
        return AxOpcodeInfo("divu", size(), left(), right());
    case AX_EXE_MDU_MUL:
        return AxOpcodeInfo("mul", size(), left(), right());
    case AX_EXE_MDU_MULU:
        return AxOpcodeInfo("mulu", size(), left(), right());
    case AX_EXE_MDU_GETMD:
        return AxOpcodeInfo("move", size(), output(), MDUReg(op.mdu_pq()));
    case AX_EXE_MDU_SETMD:
        return AxOpcodeInfo("move", size(), MDUReg(op.mdu_pq()), left());
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_lsu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto output = [op]() -> Reg
    {
        return Reg(op.reg_a());
    };

    const auto left = [op]() -> Reg
    {
        return Reg(op.reg_b());
    };

    const auto right = [op, imm24](bool imm) -> Operand
    {
        if(!imm)
        {
            return Operand(ShiftedReg(op.reg_c(), op.lsu_shift()));
        }

        const uint64_t tmp = sext_bitsize(op.lsu_imm10(), 10);
        return Operand(SImm(static_cast<int64_t>(tmp ^ (imm24 << 9))));
    };

    const auto size = [op]() -> Size
    {
        return Size(op.size());
    };

    switch(op.operation())
    {
    case AX_EXE_LSU_LD:
        return AxOpcodeInfo("ld", size(), output(), left(), right(false));
    case AX_EXE_LSU_LDS:
        return AxOpcodeInfo("lds", size(), output(), left(), right(false));
    case AX_EXE_LSU_FLD:
        return AxOpcodeInfo("fld", size(), output(), left(), right(false));
    case AX_EXE_LSU_ST:
        return AxOpcodeInfo("st", size(), output(), left(), right(false));
    case AX_EXE_LSU_FST:
        return AxOpcodeInfo("fst", size(), output(), left(), right(false));
    case AX_EXE_LSU_LDI:
        return AxOpcodeInfo("ld", size(), output(), right(true), left());
    case AX_EXE_LSU_LDIS:
        return AxOpcodeInfo("lds", size(), output(), right(true), left());
    case AX_EXE_LSU_FLDI:
        return AxOpcodeInfo("fld", size(), output(), right(true), left());
    case AX_EXE_LSU_STI:
        return AxOpcodeInfo("st", size(), output(), right(true), left());
    case AX_EXE_LSU_FSTI:
        return AxOpcodeInfo("fst", size(), output(), right(true), left());
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_fpu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto output = [op]() -> FReg
    {
        return FReg(op.reg_a());
    };

    const auto left = [op]() -> FReg
    {
        return FReg(op.reg_b());
    };

    const auto right = [op]() -> FReg
    {
        return FReg(op.reg_c());
    };

    const auto size = [op]() -> FSize
    {
        return FSize(op.size());
    };

    // Most opcodes share the same format!
    const auto format_default = [&](auto&& name, bool unary = false)
    {
        if(unary)
        {
            return AxOpcodeInfo(name, size(), output(), left());
        }

        return AxOpcodeInfo(name, size(), output(), left(), right());
    };

    const auto format_overlapped = [&](auto&& base_name, auto&& overlapped_name, bool unary = false)
    {
        if(op.size() == 3)
        {
            return AxOpcodeInfo(overlapped_name, output(), left());
        }

        return format_default(base_name, unary);
    };

    switch(op.operation())
    {
    case AX_EXE_FPU_FADD:
        static_assert(AX_EXE_FPU_FADD == AX_EXE_FPU_HTOF, "Must be overlapped!");
        return format_overlapped("fadd", "htof");
    case AX_EXE_FPU_FSUB:
        static_assert(AX_EXE_FPU_FSUB == AX_EXE_FPU_FTOH, "Must be overlapped!");
        return format_overlapped("fsub", "ftoh");
    case AX_EXE_FPU_FMUL:
        static_assert(AX_EXE_FPU_FMUL == AX_EXE_FPU_ITOF, "Must be overlapped!");
        return format_overlapped("fmul", "itof");
    case AX_EXE_FPU_FNMUL:
        static_assert(AX_EXE_FPU_FNMUL == AX_EXE_FPU_FTOI, "Must be overlapped!");
        return format_overlapped("fnmul", "ftoi");
    case AX_EXE_FPU_FMIN:
        static_assert(AX_EXE_FPU_FMIN == AX_EXE_FPU_FTOD, "Must be overlapped!");
        return format_overlapped("fmin", "ftod");
    case AX_EXE_FPU_FMAX:
        static_assert(AX_EXE_FPU_FMAX == AX_EXE_FPU_DTOF, "Must be overlapped!");
        return format_overlapped("fmax", "dtof");
    case AX_EXE_FPU_FNEG:
        static_assert(AX_EXE_FPU_FNEG == AX_EXE_FPU_ITOD, "Must be overlapped!");
        return format_overlapped("fneg", "itod", true);
    case AX_EXE_FPU_FABS:
        static_assert(AX_EXE_FPU_FABS == AX_EXE_FPU_DTOI, "Must be overlapped!");
        return format_overlapped("fabs", "dtoi", true);
    case AX_EXE_FPU_FCMOVE:
        return format_default("fcmove");
    case AX_EXE_FPU_FE:
        return format_default("fe");
    case AX_EXE_FPU_FEN:
        return format_default("fen");
    case AX_EXE_FPU_FSLT:
        return format_default("fslt");
    case AX_EXE_FPU_FMOVE:
        return format_default("fmove", true);
    case AX_EXE_FPU_FCMP:
        return AxOpcodeInfo("fcmp", size(), left(), right());
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_efu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto output = [op]() -> FReg
    {
        return FReg(op.reg_a());
    };

    const auto left = [op]() -> FReg
    {
        return FReg(op.reg_b());
    };

    const auto right = [op]() -> FReg
    {
        return FReg(op.reg_c());
    };

    const auto size = [op]() -> FSize
    {
        return FSize(op.size());
    };

    // Most opcodes share the same format!
    const auto format_default = [&](auto&& name, bool unary = false)
    {
        if(unary)
        {
            return AxOpcodeInfo(name, size(), left());
        }

        return AxOpcodeInfo(name, size(), left(), right());
    };

    switch(op.operation())
    {
    case AX_EXE_EFU_FDIV:
        format_default("fdiv");
    case AX_EXE_EFU_FATAN2:
        format_default("fatan2");
    case AX_EXE_EFU_FSQRT:
        format_default("fsqrt", true);
    case AX_EXE_EFU_FSIN:
        format_default("fsin", true);
    case AX_EXE_EFU_FATAN:
        format_default("fatan", true);
    case AX_EXE_EFU_FEXP:
        format_default("fexp", true);
    case AX_EXE_EFU_INVSQRT:
        format_default("finvsqrt", true);
    case AX_EXE_EFU_SETEF:
        return AxOpcodeInfo("setef", left());
    case AX_EXE_EFU_GETEF:
        return AxOpcodeInfo("getef", output());
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_bru_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto reg_a = [op]() -> Reg
    {
        return Reg(op.reg_a());
    };

    const auto reg_b = [op]() -> Reg
    {
        return Reg(op.reg_b());
    };

    const auto relative23 = [op, imm24]() -> RelativeLabel
    {
        return RelativeLabel(static_cast<int64_t>(sext_bitsize(op.bru_imm23(), 23) ^ (imm24 << 22)));
    };

    const auto relative24 = [op, imm24]() -> RelativeLabel
    {
        return RelativeLabel(static_cast<int64_t>(sext_bitsize(op.bru_imm24(), 24) ^ (imm24 << 23)));
    };

    const auto absolute24 = [op, imm24]() -> AbsoluteLabel
    {
        return AbsoluteLabel(op.bru_imm24() | (imm24 << 24));
    };

    switch(op.operation())
    {
    case AX_EXE_BRU_BEQ:
        return AxOpcodeInfo("beq", relative23());
    case AX_EXE_BRU_BNE:
        return AxOpcodeInfo("bne", relative23());
    case AX_EXE_BRU_BLT:
        return AxOpcodeInfo("blt", relative23());
    case AX_EXE_BRU_BGE:
        return AxOpcodeInfo("bge", relative23());
    case AX_EXE_BRU_BEQU:
        return AxOpcodeInfo("bequ", relative23());
    case AX_EXE_BRU_BNEU:
        return AxOpcodeInfo("bneu", relative23());
    case AX_EXE_BRU_BLTU:
        return AxOpcodeInfo("bltu", relative23());
    case AX_EXE_BRU_BGEU:
        return AxOpcodeInfo("bgeu", relative23());
    case AX_EXE_BRU_BRA:
        return AxOpcodeInfo("bra", relative24());
    case AX_EXE_BRU_CALLR:
        return AxOpcodeInfo("callr", relative24());
    case AX_EXE_BRU_JUMP:
        return AxOpcodeInfo("jump", absolute24());
    case AX_EXE_BRU_CALL:
        return AxOpcodeInfo("call", absolute24());
    case AX_EXE_BRU_INDIRECTCALLR:
        return AxOpcodeInfo("callr", reg_b(), reg_a());
    case AX_EXE_BRU_INDIRECTCALL:
        return AxOpcodeInfo("call", reg_b(), reg_a());
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_cu_opcode(AxOpcode op, uint64_t imm24, bool issecond)
{
    switch(op.operation())
    {
    case AX_EXE_CU_GETIR:
        return AxOpcodeInfo("getir");
    case AX_EXE_CU_SETFR:
        return AxOpcodeInfo("setfr");
    case AX_EXE_CU_MMU:
        return AxOpcodeInfo("mmu");
    case AX_EXE_CU_BRK:
        return AxOpcodeInfo("brk");
    case AX_EXE_CU_SYSCALL:
        return AxOpcodeInfo("syscall");
    case AX_EXE_CU_RETI:
        return AxOpcodeInfo("reti");
    default:
        return AxOpcodeInfo{};
    }
}

AxOpcodeInfo analyze_opcode(AxOpcode opcode, uint32_t slot, uint64_t imm24)
{
    const auto issue = (slot << 3) | opcode.unit();
    switch(issue)
    {
    case 0:
        [[fallthrough]];
    case 1:
        [[fallthrough]];
    case 8:
        [[fallthrough]];
    case 9:
        return analyze_alu_opcode(opcode, imm24, slot);
    case 2:
        [[fallthrough]];
    case 10:
        return analyze_lsu_opcode(opcode, imm24, slot);
    case 3:
        [[fallthrough]];
    case 11:
        return analyze_fpu_opcode(opcode, imm24, slot);
    case 5:
        return analyze_efu_opcode(opcode, imm24, slot);
    case 6:
        return analyze_mdu_opcode(opcode, imm24, slot);
    case 7:
        return analyze_bru_opcode(opcode, imm24, slot);
    case 13:
        return analyze_cu_opcode(opcode, imm24, slot);
    // case 14:
    //     execute_vu(opcode, imm24);
    //     break;
    default:
        return AxOpcodeInfo{};
    }
}

}

std::string AxPrettyFormatter::relative_label(uint64_t address)
{
    const auto it = m_labels.find(address);
    if(it == m_labels.end())
    {
        return add_label(m_base_address + address, fmt::format("LBB_{}", m_labels.size()));
    }

    return it->second;
}

std::string AxPrettyFormatter::absolute_label(uint64_t address)
{
    const auto it = m_labels.find(address);
    if(it == m_labels.end())
    {
        const auto& symbols = m_core->symbols();
        const auto it = std::lower_bound(symbols.begin(), symbols.end(), address, [](auto&& symbol, uint64_t address)
        {
            return symbol.address < address;
        });

        if(it != symbols.end())
        {
            if(it->address == address)
            {
                return add_label(address, fmt::format("{}", it->name));
            }

            return add_label(address, fmt::format("{}+{}", it->name, static_cast<int64_t>(address - it->address)));
        }

        return add_label(address, fmt::format("LBB_{}", m_labels.size()));
    }

    return it->second;
}

std::string AxOpcodeInfo::to_string(AxPrettyFormatter* formatter) const
{
    std::string output{}; // do not reserve, it may fit in SSO
    output += m_name;

    std::span ops = operands();
    if(!ops.empty())
    {
        if(std::holds_alternative<AxOpcodeArg::FSize>(m_operands[0].value) || std::holds_alternative<AxOpcodeArg::Size>(m_operands[0].value))
        {
            output += AxOpcodeArg::format_as(m_operands[0].value);
            ops = ops.subspan(1);
        }

        output += '\t';
    }

    for(const auto& op : ops)
    {
        if(formatter)
        {
            const auto visitors = ax_overloads{
                [](std::monostate) -> std::string
            {
                return "???";
            },
                [formatter](AxOpcodeArg::RelativeLabel label) -> std::string
            {
                return formatter->relative_label(label.value * 4ll);
            },
                [formatter](AxOpcodeArg::AbsoluteLabel label) -> std::string
            {
                return formatter->absolute_label(label.value * 4ull);
            },
                [](auto&& alternative) -> std::string
            {
                return AxOpcodeArg::format_as(alternative);
            }};

            output += std::visit(visitors, op.value);
        }
        else
        {
            output += AxOpcodeArg::format_as(op.value);
        }

        output += ',';
        output += ' ';
    }

    if(!ops.empty())
    {
        output.pop_back(); // remove comma
        output.pop_back(); // remove last tabulation
    }

    return output;
}

std::pair<AxOpcodeInfo, AxOpcodeInfo> AxOpcode::analyze(AxOpcode first, AxOpcode second)
{
    if(first.is_bundle())
    {
        const uint64_t imm24 = first.is_bundle() && second.is_moveix() ? second.moveix_imm24() : 0ull;
        return std::make_pair(
            AxOpcodeArg::analyze_opcode(first, 0, imm24),
            AxOpcodeArg::analyze_opcode(second, 1, imm24));
    }

    return std::make_pair(AxOpcodeArg::analyze_opcode(first, 0, 0), AxOpcodeInfo{});
}
