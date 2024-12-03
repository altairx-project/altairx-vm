#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <core.hpp>
#include <memory.hpp>
#include <make_opcode.hpp>

// Correctly promote a value to a register (always zext)
template<typename T>
uint64_t make_reg(T value)
{
    if constexpr(std::is_signed<T>::value)
    {
        return static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(value));
    }
    else
    {
        return static_cast<uint64_t>(value);
    }
}

TEST_CASE("Basic operations", "[basic]")
{
    AxMemory memory{8, 8, 8};
    AxCore core{memory};

    SECTION("Add big imm")
    {
        const auto addimm = make_bundle(
            make_alu_reg_imm_opcode(AX_EXE_ALU_ADD, 2, 2, 1, 0xDEADBEEE),
            make_alu_reg_imm_moveix(0xDEADBEEE)
        );

        core.registers().gpi[1] = make_reg(1);
        REQUIRE(core.execute(addimm[0], addimm[1]) == 2);
        REQUIRE(core.registers().gpi[1] == 1);
        REQUIRE(core.registers().gpi[2] == 0xDEADBEEFu);
    }
}

TEMPLATE_TEST_CASE("Conditional jumps", "[brc]", int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t)
{
    AxMemory memory{8, 8, 8};
    AxCore core{memory};

    const auto test_conditional_branch = [&](uint64_t left, uint64_t right, uint32_t op, bool expected_result, bool signed_mode, std::size_t typesize)
    {
        const auto cmp = make_alu_reg_reg_opcode(AX_EXE_ALU_CMP, std::log2(typesize), ax_no_reg, 1, 2, 0);
        const auto brc = make_bru_brc_opcode(op, 1);

        INFO("Signed version ? " << (signed_mode ? "true" : "false"));
        INFO("sizeof == " << typesize);
        INFO("left == " << left);
        INFO("right == " << right);
        INFO("opcode: " << brc.to_string());
        INFO("expected_result == " << expected_result);

        core.registers().gpi[1] = left;
        core.registers().gpi[2] = right;
        REQUIRE(core.execute(cmp, make_noop_opcode()) == 1);
        core.registers().pc = 42;
        if(expected_result)
        {
            REQUIRE(core.execute(brc, make_noop_opcode()) == 0);
            REQUIRE(core.registers().pc == 43);
        }
        else
        {
            REQUIRE(core.execute(brc, make_noop_opcode()) == 1);
            REQUIRE(core.registers().pc == 42);
        }
    };

    if constexpr(std::is_signed_v<TestType>)
    {
        SECTION("Checking for signed types")
        {
            const auto check_for = [&](TestType left, TestType right)
            {
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BEQ, left == right, true, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BNE, left != right, true, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BLT, left < right, true, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BGE, left >= right, true, sizeof(left));
            };

            SECTION("Checking edge values (0, -1, min, max)")
            {
                check_for(TestType(0), TestType(0));
                check_for(TestType(-1), TestType(-1));
                check_for(TestType(0), TestType(-1));
                check_for(TestType(-1), TestType(0));
                check_for(std::numeric_limits<TestType>::max(), TestType(0));
                check_for(std::numeric_limits<TestType>::max(), TestType(-1));
                check_for(std::numeric_limits<TestType>::min(), TestType(0));
                check_for(std::numeric_limits<TestType>::min(), TestType(-1));
                check_for(TestType(0), std::numeric_limits<TestType>::max());
                check_for(TestType(-1), std::numeric_limits<TestType>::max());
                check_for(TestType(0), std::numeric_limits<TestType>::min());
                check_for(TestType(-1), std::numeric_limits<TestType>::min());
                check_for(std::numeric_limits<TestType>::max(), std::numeric_limits<TestType>::max());
                check_for(std::numeric_limits<TestType>::max(), std::numeric_limits<TestType>::min());
                check_for(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max());
                check_for(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::min());
            }

            SECTION("Checking random values")
            {
                const auto left = GENERATE(take(4, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max())));
                const auto right = GENERATE(take(4, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max())));
                check_for(left, right);
            }
        }
    }
    else
    {
        SECTION("Checking for unsigned types")
        {
            const auto check_for = [&](TestType left, TestType right)
            {
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BEQ, left == right, false, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BNE, left != right, false, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BLTU, left < right, false, sizeof(left));
                test_conditional_branch(make_reg(left), make_reg(right), AX_EXE_BRU_BGEU, left >= right, false, sizeof(left));
            };

            SECTION("Checking edge values (0, max)")
            {
                check_for(TestType(0), TestType(0));
                check_for(std::numeric_limits<TestType>::max(), TestType(0));
                check_for(TestType(0), std::numeric_limits<TestType>::max());
            }

            SECTION("Checking random values")
            {
                const auto left = GENERATE(take(4, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max())));
                const auto right = GENERATE(take(4, random(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max())));
                check_for(left, right);
            }
        }
    }
}
