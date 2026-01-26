// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#ifndef AXOPCODE_HPP_INCLUDED
#define AXOPCODE_HPP_INCLUDED

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <array>
#include <span>

namespace AxOpcodeArg
{
struct Reg
{
    uint32_t id{};
    explicit Reg(uint32_t r) noexcept
        : id{r}
    {
    }
};

std::string format_as(Reg r);

struct FReg
{
    uint32_t id{};
    explicit FReg(uint32_t r) noexcept
        : id{r}
    {
    }
};

std::string format_as(FReg r);

struct MDUReg
{
    uint32_t id{};
    explicit MDUReg(uint32_t r) noexcept
        : id{r}
    {
    }
};

std::string format_as(MDUReg r);

struct SImm
{
    int64_t value{};
    explicit SImm(int64_t val) noexcept
        : value{val}
    {
    }
};

std::string format_as(SImm r);

struct AbsoluteLabel
{
    uint64_t value{};
    explicit AbsoluteLabel(uint64_t val) noexcept
        : value{val}
    {
    }
};

std::string format_as(AbsoluteLabel label);

struct RelativeLabel
{
    int64_t value{};
    explicit RelativeLabel(int64_t val) noexcept
        : value{val}
    {
    }
};

std::string format_as(RelativeLabel label);

struct UImm
{
    uint64_t value{};
    bool hexa_decimal{};
    explicit UImm(uint64_t val, bool hexa = false) noexcept
        : value{val}
        , hexa_decimal{hexa}
    {
    }
};

std::string format_as(UImm r);

struct Size
{
    explicit Size(uint32_t val) noexcept
        : value{val}
    {
    }

    uint32_t value{};
};

std::string format_as(Size r);

struct FSize
{
    explicit FSize(uint32_t val) noexcept
        : value{val}
    {
    }

    uint32_t value{};
};

std::string format_as(FSize r);

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

std::string format_as(ShiftedReg r);

struct Operand
{
    Operand() noexcept = default;

    template<typename T>
    Operand(T&& val)
        : value{std::forward<T>(val)}
    {
    }

    Operand(const Operand& val) = default;
    Operand& operator=(const Operand& val) = default;
    Operand(Operand&& val) noexcept = default;
    Operand& operator=(Operand&& val) noexcept = default;

    std::variant<std::monostate, Reg, FReg, MDUReg, ShiftedReg, SImm, UImm, RelativeLabel, AbsoluteLabel, Size, FSize> value;
};

std::string format_as(Operand r);
};

class AxCore;

class AxPrettyFormatter
{
public:
    using label_map = std::unordered_map<uint64_t, std::string>;

    explicit AxPrettyFormatter(const AxCore& core)
        : m_core{&core}
    {
    }

    // Must be called everytime
    void set_base_address(uint64_t base_address) noexcept
    {
        m_base_address = base_address;
    }

    std::string relative_label(uint64_t offset);
    std::string absolute_label(uint64_t address);

    const label_map& labels() const noexcept
    {
        return m_labels;
    }

private:
    std::string add_label(uint64_t address, const std::string& name)
    {
        return m_labels.emplace(address, name).first->second;
    }

    const AxCore* m_core;
    label_map m_labels;
    uint64_t m_base_address{};
};

class AxOpcodeInfo
{
public:
    explicit AxOpcodeInfo() noexcept = default;

    explicit AxOpcodeInfo(const char* name) noexcept
        : m_name{name}
    {
    }

    template<typename... Args>
    explicit AxOpcodeInfo(const char* name, Args&&... args) noexcept
        : m_name{name}
        , m_count{sizeof...(Args)}
    {
        static_assert(sizeof...(Args) <= operand_limit);
        fill_operands<0>(std::forward<Args>(args)...);
    }

    bool valid() const noexcept
    {
        return !(m_name.empty() && m_count == 0);
    }

    // Real string view, not always null-terminated!
    std::string_view name() const noexcept
    {
        return m_name;
    }

    std::span<const AxOpcodeArg::Operand> operands() const noexcept
    {
        return std::span<const AxOpcodeArg::Operand>{m_operands.data(), m_count};
    }

    std::string to_string(AxPrettyFormatter* formatter = nullptr) const;

private:
    template<std::size_t index, typename T, typename... Args>
    void fill_operands(T&& value, Args&&... args)
    {
        fill_operands<index>(std::forward<T>(value));
        return fill_operands<index + 1>(std::forward<Args>(args)...);
    }

    template<std::size_t index, typename T>
    void fill_operands(T&& value)
    {
        m_operands[index] = std::forward<T>(value);
    }

    static constexpr std::size_t operand_limit = 4;

    std::string_view m_name{};
    std::size_t m_count{};
    std::array<AxOpcodeArg::Operand, operand_limit> m_operands;
};

enum class SyscallId : uint64_t
{
    exit = 1,        // code
    stdio_read = 2,  // fb, buf, size
    stdio_write = 3, // fb, buf, size
};

enum AxOpcodes : uint32_t
{
    //------------- ALU(0) -----
    AX_EXE_ALU_MOVEIX = 0x00,
    AX_EXE_ALU_MOVEI,
    AX_EXE_ALU_EXT,
    AX_EXE_ALU_INS,

    AX_EXE_ALU_MAX,
    AX_EXE_ALU_UMAX,
    AX_EXE_ALU_MIN,
    AX_EXE_ALU_UMIN,

    AX_EXE_ALU_ADDS,
    AX_EXE_ALU_SUBS,
    AX_EXE_ALU_RTL,
    AX_EXE_ALU_RTR,

    AX_EXE_ALU_CMP,
    AX_EXE_ALU_BIT,
    AX_EXE_ALU_TEST,
    AX_EXE_ALU_TESTFR,

    //------------- ALU(1) -----

    AX_EXE_ALU_ADD = 0x10,
    AX_EXE_ALU_SUB,
    AX_EXE_ALU_XOR,
    AX_EXE_ALU_OR,

    AX_EXE_ALU_AND,
    AX_EXE_ALU_LSL,
    AX_EXE_ALU_ASR,
    AX_EXE_ALU_LSR,

    AX_EXE_ALU_SE,
    AX_EXE_ALU_SEN,
    AX_EXE_ALU_SLTS,
    AX_EXE_ALU_SLTU,

    AX_EXE_ALU_SAND,
    AX_EXE_ALU_SBIT,
    AX_EXE_ALU_CMOVEN,
    AX_EXE_ALU_CMOVE,

    //------------- LSU(2) -------
    AX_EXE_LSU_LD = 0x20,
    AX_EXE_LSU_LDS,
    AX_EXE_LSU_FLD,
    AX_EXE_LSU_EMPTY0,

    AX_EXE_LSU_ST,
    AX_EXE_LSU_EMPTY1,
    AX_EXE_LSU_FST,
    AX_EXE_LSU_EMPTY2,

    AX_EXE_LSU_LDI,
    AX_EXE_LSU_LDIS,
    AX_EXE_LSU_FLDI,
    AX_EXE_LSU_EMPTY3,

    AX_EXE_LSU_STI,
    AX_EXE_LSU_EMPTY4,
    AX_EXE_LSU_FSTI,
    AX_EXE_LSU_EMPTY5,

    //------------- FPU(3) -----
    AX_EXE_FPU_FADD = 0x30,
    AX_EXE_FPU_FSUB,
    AX_EXE_FPU_FMUL,
    AX_EXE_FPU_FNMUL,

    AX_EXE_FPU_FMIN,
    AX_EXE_FPU_FMAX,
    AX_EXE_FPU_FNEG,
    AX_EXE_FPU_FABS,

    // casts overlap with previous opcode (with size == 3)
    AX_EXE_FPU_HTOF = AX_EXE_FPU_FADD,
    AX_EXE_FPU_FTOH = AX_EXE_FPU_FSUB,
    AX_EXE_FPU_ITOF = AX_EXE_FPU_FMUL,
    AX_EXE_FPU_FTOI = AX_EXE_FPU_FNMUL,
    AX_EXE_FPU_FTOD = AX_EXE_FPU_FMIN,
    AX_EXE_FPU_DTOF = AX_EXE_FPU_FMAX,
    AX_EXE_FPU_ITOD = AX_EXE_FPU_FNEG,
    AX_EXE_FPU_DTOI = AX_EXE_FPU_FABS,

    AX_EXE_FPU_FCMOVE,
    AX_EXE_FPU_FE,
    AX_EXE_FPU_FEN,
    AX_EXE_FPU_FSLT,

    AX_EXE_FPU_FMOVE,
    AX_EXE_FPU_FCMP,
    AX_EXE_FPU_FMOVEI,
    AX_EXE_FPU_FCMPI,

    //------------- EFU(5) -----
    AX_EXE_EFU_FDIV = 0x50,
    AX_EXE_EFU_FATAN2,
    AX_EXE_EFU_FSQRT,
    AX_EXE_EFU_FSIN,

    AX_EXE_EFU_FATAN,
    AX_EXE_EFU_FEXP,
    AX_EXE_EFU_INVSQRT,
    AX_EXE_EFU_EMPTY0,

    AX_EXE_EFU_EMPTY1,
    AX_EXE_EFU_EMPTY2,
    AX_EXE_EFU_EMPTY3,
    AX_EXE_EFU_EMPTY4,

    AX_EXE_EFU_EMPTY5,
    AX_EXE_EFU_EMPTY6,
    AX_EXE_EFU_SETEF,
    AX_EXE_EFU_GETEF,

    //------------- MDU(6) -----
    AX_EXE_MDU_DIV = 0x60,
    AX_EXE_MDU_DIVU,
    AX_EXE_MDU_MUL,
    AX_EXE_MDU_MULU,

    AX_EXE_MDU_GETMD,
    AX_EXE_MDU_SETMD,
    AX_EXE_MDU_EMPTY0,
    AX_EXE_MDU_EMPTY1,

    AX_EXE_MDU_EMPTY2,
    AX_EXE_MDU_EMPTY3,
    AX_EXE_MDU_EMPTY4,
    AX_EXE_MDU_EMPTY5,

    AX_EXE_MDU_EMPTY6,
    AX_EXE_MDU_EMPTY7,
    AX_EXE_MDU_EMPTY8,
    AX_EXE_MDU_EMPTY9,

    //------------- BRU(7) -----
    AX_EXE_BRU_BEQ = 0x70,
    AX_EXE_BRU_BNE,
    AX_EXE_BRU_BLT,
    AX_EXE_BRU_BGE,

    AX_EXE_BRU_BEQU,
    AX_EXE_BRU_BNEU,
    AX_EXE_BRU_BLTU,
    AX_EXE_BRU_BGEU,

    AX_EXE_BRU_BRA,
    AX_EXE_BRU_CALLR,
    AX_EXE_BRU_JUMP,
    AX_EXE_BRU_CALL,

    AX_EXE_BRU_EMPTY2,
    AX_EXE_BRU_INDIRECTCALLR,
    AX_EXE_BRU_EMPTY3,
    AX_EXE_BRU_INDIRECTCALL,

    //------------- CU(5) -----
    AX_EXE_CU_GETIR = 0x50,
    AX_EXE_CU_SETFR,
    AX_EXE_CU_EMPTY0,
    AX_EXE_CU_MMU,

    AX_EXE_CU_EMPTY1,
    AX_EXE_CU_EMPTY2,
    AX_EXE_CU_EMPTY3,
    AX_EXE_CU_EMPTY4,

    AX_EXE_CU_EMPTY5,
    AX_EXE_CU_EMPTY6,
    AX_EXE_CU_EMPTY7,
    AX_EXE_CU_EMPTY8,
    AX_EXE_CU_EMPTY9,
    AX_EXE_CU_BRK,
    AX_EXE_CU_SYSCALL,
    AX_EXE_CU_RETI,

    //------------- VU(5) -----
    AX_EXE_VU_VECTOR2 = 0x60,
    AX_EXE_VU_VECTOR4,
    AX_EXE_VU_VECTOR8,
    AX_EXE_VU_INV,

    AX_EXE_VU_VECTOR2X2,
    AX_EXE_VU_VECTOR4X2,
    AX_EXE_VU_VECTOR8X2,
    AX_EXE_VU_INVX2
};

struct AxOpcode
{
    uint32_t value{};

    constexpr AxOpcode() noexcept = default;

    // implicit conversions
    constexpr AxOpcode(uint32_t val) noexcept
        : value{val}
    {
    }

    constexpr operator uint32_t() const noexcept
    {
        return value;
    }

    uint32_t opcode() const noexcept
    {
        return (value >> 1) & 0x0Fu;
    }

    uint32_t unit() const noexcept
    {
        return (value >> 5) & 0x07u;
    }

    // opcode + unit
    uint32_t operation() const noexcept
    {
        return (value >> 1) & 0x7Fu;
    }

    uint32_t size() const noexcept
    {
        return (value >> 8) & 0x03u;
    }

    uint32_t reg_c() const noexcept
    {
        return (value >> 14) & 0x3Fu;
    }

    uint32_t reg_b() const noexcept
    {
        return (value >> 20) & 0x3Fu;
    }

    uint32_t reg_a() const noexcept
    {
        return (value >> 26) & 0x3Fu;
    }

    bool alu_has_imm() const noexcept
    {
        return (value & 0x0400u) != 0;
    }

    uint64_t alu_shift() const noexcept
    {
        return (value >> 11) & 0x07u;
    }

    uint64_t alu_imm9() const noexcept
    {
        return (value >> 11) & 0x01FFu;
    }

    uint64_t ext_ins_imm1() const noexcept
    {
        return (value >> 8) & 0x3Fu;
    }

    uint64_t ext_ins_imm2() const noexcept
    {
        return (value >> 14) & 0x3Fu;
    }

    uint64_t alu_move_imm() const noexcept
    {
        return (value >> 8) & 0x0003FFFFu;
    }

    uint32_t mdu_pq() const noexcept
    {
        return (value >> 11) & 0x03U;
    }

    uint64_t lsu_shift() const noexcept
    {
        return (value >> 11) & 0x07u;
    }

    uint64_t lsu_imm10() const noexcept
    {
        return (value >> 10) & 0x03FFu;
    }

    uint64_t lsu_imm16() const noexcept
    {
        return (value >> 10) & 0x0FFFFu;
    }

    uint64_t moveix_imm24() const noexcept
    {
        return (value >> 8) & 0x00FFFFFFu;
    }

    uint64_t bru_imm23() const noexcept
    {
        return (value >> 9) & 0x007FFFFFu;
    }

    uint64_t bru_imm24() const noexcept
    {
        return (value >> 8) & 0x00FFFFFFu;
    }

    // return true if next opcode is bundled with this one.
    bool is_bundle() const noexcept
    {
        return (value & 1) != 0;
    }

    bool is_moveix() const noexcept
    {
        return (value & 0xFE) == 0;
    }

    bool is_noop() const noexcept
    {
        return value == 0;
    }

    bool is_ret() const noexcept
    {
        // call lr, zero (reversed operand order)
        return (operation() == AX_EXE_BRU_INDIRECTCALL && reg_a() == 63 && reg_b() == 31);
    }

    static std::pair<AxOpcodeInfo, AxOpcodeInfo> analyze(AxOpcode first, AxOpcode second);
};

#endif // !AXOPCODE_HPP_INCLUDED
