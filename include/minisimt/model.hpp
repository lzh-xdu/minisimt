#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace minisimt {

inline constexpr std::size_t kRegisterCount = 8;

enum class Opcode {
    MovImm,
    MovReg,
    Add,
    Exit,
};

enum class StepResult {
    Executed,
    Halted,
    Error,
};

struct Instruction {
    Opcode opcode;
    int dst{-1};
    int src1{-1};
    int src2{-1};
    int immediate{0};

    [[nodiscard]] static constexpr Instruction mov_imm(
        int dst_register,
        int value) noexcept
    {
        return {Opcode::MovImm, dst_register, -1, -1, value};
    }

    [[nodiscard]] static constexpr Instruction mov_reg(
        int dst_register,
        int src_register) noexcept
    {
        return {Opcode::MovReg, dst_register, src_register, -1, 0};
    }

    [[nodiscard]] static constexpr Instruction add(
        int dst_register,
        int lhs_register,
        int rhs_register) noexcept
    {
        return {
            Opcode::Add,
            dst_register,
            lhs_register,
            rhs_register,
            0,
        };
    }

    [[nodiscard]] static constexpr Instruction exit() noexcept
    {
        return {Opcode::Exit, -1, -1, -1, 0};
    }
};

struct ThreadContext {
    std::array<int, kRegisterCount> registers{};
    std::size_t pc{0};
    bool finished{false};

    bool operator==(const ThreadContext&) const = default;
};

using Program = std::vector<Instruction>;

[[nodiscard]] StepResult step(
    const Program& program,
    ThreadContext& context) noexcept;

[[nodiscard]] std::string opcode_name(Opcode opcode);
[[nodiscard]] std::string step_result_name(StepResult result);
[[nodiscard]] std::string format_instruction(const Instruction& instruction);
[[nodiscard]] std::string format_context(const ThreadContext& context);

} // namespace minisimt
