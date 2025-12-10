// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "opcode.hpp"

#include <fmt/format.h>
#include <variant>

#include "utilities.hpp"
#include "panic.hpp"

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

std::string format_as(SImm imm)
{
    return fmt::format("{}", imm.value);
}

std::string format_as(UImm imm)
{
    if(imm.hexa_decimal)
    {
      return fmt::format("{0:#x}", imm.value);
    }

    return fmt::format("{}", imm.value);
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
    const auto visitors = ax_overloads
    {
        [](std::monostate) -> std::string
        {
            return "???";
        },
            [](auto&& alternative) -> std::string
        {
            return fmt::format("{}", alternative);
        }
    };

    return std::visit(visitors, op.value);
}

struct StringFormatter
{
    using result_type = std::string;

    template<typename... Args>
    result_type operator()(fmt::format_string<Args...> format, Args&&... args)
    {
        return fmt::format(format, std::forward<Args>(args)...);
    }
};

struct InfoFormatter
{
    using result_type = AxOpcodeInfo;

    template<typename... Args>
    result_type operator()(const char* name, Args&&... args)
    {
        return result_type{name, std::forward<Args>(args)...};
    }

    // Some formats uses an indirect way to specify the name,
    // using formatter("{}", name) instead of formatter("name")
    template<typename... Args>
    result_type operator()(const char*, const char* name, Args&&... args)
    {
        return result_type{name, std::forward<Args>(args)...};
    }
};

template<typename Formatter>
typename Formatter::result_type analyze_alu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
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
        return formatter("{}{}\t{}, {}, {}", name, size(), output(), left(), right());
    };

    switch(op.operation())
    {
    case AX_EXE_ALU_MOVEIX: // no-op
        return issecond ? formatter("moveix") : formatter("nop");
    case AX_EXE_ALU_MOVEI:
        return formatter("movei\t{}, {}", output(), SImm(static_cast<int64_t>(sext_bitsize(op.alu_move_imm(), 18) ^ (imm24 << 17))));
    case AX_EXE_ALU_EXT:
        return formatter("ext\t{}, {}, {}, {}", output(), left(), UImm(op.ext_ins_imm1()), UImm(op.ext_ins_imm2()));
    case AX_EXE_ALU_INS:
        return formatter("ins\t{}, {}, {}, {}", output(), left(), UImm(op.ext_ins_imm1()), UImm(op.ext_ins_imm2()));

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
        return formatter("cmp{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_BIT:
        return formatter("bit{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_TEST:
        return formatter("test{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_TESTFR:
        return formatter("testfr{}\t{}", size(), right());

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
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_mdu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
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
        return formatter("div{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_DIVU:
        return formatter("divu{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_MUL:
        return formatter("mul{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_MULU:
        return formatter("mulu{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_GETMD:
        return formatter("move{}\t{}, {}", size(), output(), MDUReg(op.mdu_pq()));
    case AX_EXE_MDU_SETMD:
        return formatter("move{}\t{}, {}", size(), MDUReg(op.mdu_pq()), left());
    default:
        return typename Formatter::result_type{};
    }
}
template<typename Formatter>
typename Formatter::result_type analyze_lsu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
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
        return formatter("ld{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_LDS:
        return formatter("lds{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_FLD:
        return formatter("fld{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_ST:
        return formatter("st{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_FST:
        return formatter("fst{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_LDI:
        return formatter("ld{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_LDIS:
        return formatter("lds{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_FLDI:
        return formatter("fld{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_STI:
        return formatter("st{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_FSTI:
        return formatter("fst{}\t{}, {}[{}]", size(), output(), right(true), left());
    default:
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_fpu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
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
            return formatter("{}{}\t{}, {}", name, size(), output(), left());
        }

        return formatter("{}{}\t{}, {}, {}", name, size(), output(), left(), right());
    };

    const auto format_overlapped = [&](auto&& base_name, auto&& overlapped_name, bool unary = false)
    {
        if(op.size() == 3)
        {
            return formatter("{}\t {}, {}", overlapped_name, output(), left());
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
        return formatter("fcmp{}\t{}, {}", size(), left(), right());
    default:
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_efu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
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
            return formatter("{}{}\t{}", name, size(), left());
        }

        return formatter("{}{}\t{}, {}", name, size(), left(), right());
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
        return formatter("setef\t{}", left());
    case AX_EXE_EFU_GETEF:
        return formatter("getef\t{}", output());
    default:
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_bru_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
{
    const auto reg_a = [op]() -> Reg
    {
        return Reg(op.reg_a());
    };

    const auto reg_b = [op]() -> Reg
    {
        return Reg(op.reg_b());
    };

    const auto relative23 = [op, imm24]() -> SImm
    {
        return SImm(static_cast<int64_t>(sext_bitsize(op.bru_imm23(), 23) ^ (imm24 << 22)));
    };

    const auto relative24 = [op, imm24]() -> SImm
    {
        return SImm(static_cast<int64_t>(sext_bitsize(op.bru_imm24(), 24) ^ (imm24 << 23)));
    };

    const auto absolute24 = [op, imm24]() -> UImm
    {
        return UImm(op.bru_imm24() | (imm24 << 24), true);
    };

    switch(op.operation())
    {
    case AX_EXE_BRU_BEQ:
        return formatter("beq\t{}", relative23());
    case AX_EXE_BRU_BNE:
        return formatter("bne\t{}", relative23());
    case AX_EXE_BRU_BLT:
        return formatter("blt\t{}", relative23());
    case AX_EXE_BRU_BGE:
        return formatter("bge\t{}", relative23());
    case AX_EXE_BRU_BEQU:
        return formatter("bequ\t{}", relative23());
    case AX_EXE_BRU_BNEU:
        return formatter("bneu\t{}", relative23());
    case AX_EXE_BRU_BLTU:
        return formatter("bltu\t{}", relative23());
    case AX_EXE_BRU_BGEU:
        return formatter("bgeu\t{}", relative23());
    case AX_EXE_BRU_BRA:
        return formatter("bra\t{}", relative24());
    case AX_EXE_BRU_CALLR:
        return formatter("callr\t{}", relative24());
    case AX_EXE_BRU_JUMP:
        return formatter("jump\t{}", absolute24());
    case AX_EXE_BRU_CALL:
        return formatter("call\t{}", absolute24());
    case AX_EXE_BRU_INDIRECTCALLR:
        return formatter("callr\t{}, {}", reg_b(), reg_a());
    case AX_EXE_BRU_INDIRECTCALL:
        return formatter("call\t{}, {}", reg_b(), reg_a());
    default:
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_cu_opcode(Formatter formatter, AxOpcode op, uint64_t imm24, bool issecond)
{
    switch(op.operation())
    {
    case AX_EXE_CU_GETIR:
        return formatter("getir");
    case AX_EXE_CU_SETFR:
        return formatter("setfr");
    case AX_EXE_CU_MMU:
        return formatter("mmu");
    case AX_EXE_CU_BRK:
        return formatter("brk");
    case AX_EXE_CU_SYSCALL:
        return formatter("syscall");
    case AX_EXE_CU_RETI:
        return formatter("reti");
    default:
        return typename Formatter::result_type{};
    }
}

template<typename Formatter>
typename Formatter::result_type analyze_opcode(Formatter formatter, AxOpcode opcode, uint32_t slot, uint64_t imm24)
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
        return analyze_alu_opcode(formatter, opcode, imm24, slot);
    case 2:
        [[fallthrough]];
    case 10:
        return analyze_lsu_opcode(formatter, opcode, imm24, slot);
    case 3:
        [[fallthrough]];
    case 11:
        return analyze_fpu_opcode(formatter, opcode, imm24, slot);
    case 5:
        return analyze_efu_opcode(formatter, opcode, imm24, slot);
    case 6:
        return analyze_mdu_opcode(formatter, opcode, imm24, slot);
    case 7:
        return analyze_bru_opcode(formatter, opcode, imm24, slot);
    case 13:
        return analyze_cu_opcode(formatter, opcode, imm24, slot);
    // case 14:
    //     execute_vu(opcode, imm24);
    //     break;
    default:
        return typename Formatter::result_type{};
    }
}

}

std::pair<AxOpcodeInfo, AxOpcodeInfo> AxOpcode::analyze(AxOpcode first, AxOpcode second)
{
    using namespace AxOpcodeArg;

    if(first.is_bundle())
    {
        const uint64_t imm24 = first.is_bundle() && second.is_moveix() ? second.moveix_imm24() : 0ull;
        return std::make_pair(
            analyze_opcode(InfoFormatter{}, first, 0, imm24),
            analyze_opcode(InfoFormatter{}, second, 1, imm24));
    }

    return std::make_pair(analyze_opcode(InfoFormatter{}, first, 0, 0), AxOpcodeInfo{});
}

std::pair<std::string, std::string> AxOpcode::to_string(AxOpcode first, AxOpcode second)
{
    using namespace AxOpcodeArg;

    if(first.is_bundle())
    {
        const uint64_t imm24 = first.is_bundle() && second.is_moveix() ? second.moveix_imm24() : 0ull;
        return std::make_pair(
            analyze_opcode(StringFormatter{}, first, 0, imm24),
            analyze_opcode(StringFormatter{}, second, 1, imm24));
    }

    return std::make_pair(analyze_opcode(StringFormatter{}, first, 0, 0), std::string{});
}
