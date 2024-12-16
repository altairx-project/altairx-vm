#include "opcode.hpp"

#include <fmt/format.h>
#include <variant>

#include "utilities.hpp"
#include "panic.hpp"

namespace
{

struct Reg
{
    uint32_t id{};
    explicit Reg(uint32_t r) noexcept
        : id{r}
    {
    }
};

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

struct MDUReg
{
    uint32_t id{};
    explicit MDUReg(uint32_t r) noexcept
        : id{r}
    {
    }
};

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

struct SImm
{
    int64_t value{};
    explicit SImm(int64_t val) noexcept
        : value{val}
    {
    }
};

std::string format_as(SImm imm)
{
    return fmt::format("{}", imm.value);
}

struct UImm
{
    uint64_t value{};
    explicit UImm(uint64_t val) noexcept
        : value{val}
    {
    }
};

std::string format_as(UImm imm)
{
    return fmt::format("{}", imm.value);
}

struct Size
{
    explicit Size(uint32_t val) noexcept
        : value{val}
    {
    }

    uint32_t value{};
};

std::string format_as(Size size) noexcept
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

struct ShiftedReg
{
    explicit ShiftedReg(uint32_t r, uint32_t val) noexcept
        : reg{r}
        , shift{val}
    {
    }

    uint32_t reg{};
    uint32_t shift{};
};

std::string format_as(ShiftedReg shift) noexcept
{
    if(shift.shift > 0)
    {
        return fmt::format("{} << {}", Reg(shift.reg), shift.shift);
    }

    return fmt::format("{}", Reg(shift.reg));
}

struct Operand
{
    template<typename T>
    explicit Operand(T&& val)
        : value{std::forward<T>(val)}
    {
    }

    std::variant<Reg, MDUReg, ShiftedReg, SImm, UImm, Size> value;
};

std::string format_as(Operand op) noexcept
{
    return std::visit([](auto&& alternative) -> std::string
        {
        return fmt::format("{}", alternative);
    },
        op.value);
}

std::string alu_opcode_to_string(AxOpcode op, uint64_t imm24, bool issecond)
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
        return fmt::format("{}{}\t{}, {}, {}", name, size(), output(), left(), right());
    };

    switch(op.operation())
    {
    case AX_EXE_ALU_MOVEIX: // no-op
        return issecond ? "moveix" : "nop";
    case AX_EXE_ALU_MOVEI:
        return fmt::format("movei\t{}, {}", output(), static_cast<int64_t>(sext_bitsize(op.alu_move_imm(), 18) ^ (imm24 << 18)));
    case AX_EXE_ALU_EXT:
        return fmt::format("ext\t{}, {}, {}, {}", output(), left(), op.ext_ins_imm1(), op.ext_ins_imm2());
    case AX_EXE_ALU_INS:
        return fmt::format("ins\t{}, {}, {}, {}", output(), left(), op.ext_ins_imm1(), op.ext_ins_imm2());

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
        return fmt::format("cmp{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_BIT:
        return fmt::format("bit{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_TEST:
        return fmt::format("test{}\t{}, {}", size(), left(), right());
    case AX_EXE_ALU_TESTFR:
        return fmt::format("testfr{}\t{}", size(), right());

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
        return {};
    }
}

std::string mdu_opcode_to_string(AxOpcode op, uint64_t imm24, bool issecond)
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
        return fmt::format("div{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_DIVU:
        return fmt::format("divu{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_MUL:
        return fmt::format("mul{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_MULU:
        return fmt::format("mulu{}\t{}, {}", size(), left(), right());
    case AX_EXE_MDU_GETMD:
        return fmt::format("move{}\t{}, {}", size(), output(), MDUReg(op.mdu_pq()));
    case AX_EXE_MDU_SETMD:
        return fmt::format("move{}\t{}, {}", size(), MDUReg(op.mdu_pq()), left());
    default:
        return {};
    }
}

std::string lsu_opcode_to_string(AxOpcode op, uint64_t imm24, bool issecond)
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
        return fmt::format("ld{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_LDS:
        return fmt::format("lds{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_FLD:
        ax_panic("AX_EXE_LSU_FLD not implemented");
    case AX_EXE_LSU_ST:
        return fmt::format("st{}\t{}, {}[{}]", size(), output(), left(), right(false));
    case AX_EXE_LSU_FST:
        ax_panic("AX_EXE_LSU_FST not implemented");
    case AX_EXE_LSU_LDI:
        return fmt::format("lds{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_LDIS:
        return fmt::format("lds{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_FLDI:
        ax_panic("AX_EXE_LSU_FLDI not implemented");
    case AX_EXE_LSU_STI:
        return fmt::format("st{}\t{}, {}[{}]", size(), output(), right(true), left());
    case AX_EXE_LSU_FSTI:
        ax_panic("AX_EXE_LSU_FSTI not implemented");
    default:
        return {};
    }
}

std::string bru_opcode_to_string(AxOpcode op, uint64_t imm24, bool issecond)
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
        return UImm(op.bru_imm24() | (imm24 << 24));
    };

    switch(op.operation())
    {
    case AX_EXE_BRU_BEQ:
        return fmt::format("beq\t{}", relative23());
    case AX_EXE_BRU_BNE:
        return fmt::format("bne\t{}", relative23());
    case AX_EXE_BRU_BLT:
        return fmt::format("blt\t{}", relative23());
    case AX_EXE_BRU_BGE:
        return fmt::format("bge\t{}", relative23());
    case AX_EXE_BRU_BLTU:
        return fmt::format("bltu\t{}", relative23());
    case AX_EXE_BRU_BGEU:
        return fmt::format("bgeu\t{}", relative23());
    case AX_EXE_BRU_BRA:
        return fmt::format("bra\t{}", relative24());
    case AX_EXE_BRU_CALLR:
        return fmt::format("callr\t{}", relative24());
    case AX_EXE_BRU_JUMP:
        return fmt::format("jump\t{}", absolute24());
    case AX_EXE_BRU_CALL:
        return fmt::format("call\t{}", absolute24());
    case AX_EXE_BRU_INDIRECTCALLR:
        return fmt::format("callr\t{}, {}", reg_b(), reg_a());
    case AX_EXE_BRU_INDIRECTCALL:
        return fmt::format("call\t{}, {}", reg_b(), reg_a());
    default:
        return {};
    }
}

std::string opcode_to_string(AxOpcode opcode, uint32_t slot, uint64_t imm24)
{
    const auto issue = (slot << 3) | opcode.unit();
    switch(issue)
    {
    case 0:
        [[fallthrough]];
    case 1:
        return alu_opcode_to_string(opcode, imm24, false);
    case 8:
        [[fallthrough]];
    case 9:
        return alu_opcode_to_string(opcode, imm24, true);
    case 2:
        return lsu_opcode_to_string(opcode, imm24, false);
    case 10:
        return lsu_opcode_to_string(opcode, imm24, true);
    // case 3:
    //     [[fallthrough]];
    // case 11:
    //     execute_fpu(opcode, imm24);
    //     break;
    // case 5:
    //     execute_efu(opcode, imm24);
    //     break;
    case 6:
        return mdu_opcode_to_string(opcode, imm24, false);
    case 7:
        return bru_opcode_to_string(opcode, imm24, false);
    // case 13:
    //     execute_cu(opcode, imm24);
    //     break;
    // case 14:
    //     execute_vu(opcode, imm24);
    //     break;
    default:
        return {};
    }
}

}

std::pair<std::string, std::string> AxOpcode::to_string(AxOpcode first, AxOpcode second)
{
    if(first.is_bundle())
    {
        const uint64_t imm24 = first.is_bundle() && second.is_moveix() ? second.moveix_imm24() : 0ull;
        return std::make_pair(opcode_to_string(first, 0, imm24), opcode_to_string(second, 1, imm24));
    }

    return std::make_pair(opcode_to_string(first, 0, 0), std::string{});
}
