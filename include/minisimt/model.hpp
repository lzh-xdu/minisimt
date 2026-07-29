#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <string>
#include <vector>

namespace minisimt {

inline constexpr std::size_t kRegisterCount = 8;
inline constexpr std::size_t kWarpLaneCount = 4;

using RegisterFile = std::array<int, kRegisterCount>;
using ActiveMask = std::bitset<kWarpLaneCount>;
using GlobalMemory = std::vector<int>;

enum class Opcode {
    MovImm,
    MovReg,
    Add,
    Mul,
    LaneId,
    Load,
    Store,
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

    bool operator==(const Instruction&) const = default;

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

    [[nodiscard]] static constexpr Instruction mul(
        int dst_register,
        int lhs_register,
        int rhs_register) noexcept
    {
        return {
            Opcode::Mul,
            dst_register,
            lhs_register,
            rhs_register,
            0,
        };
    }

    [[nodiscard]] static constexpr Instruction lane_id(
        int dst_register) noexcept
    {
        return {Opcode::LaneId, dst_register, -1, -1, 0};
    }

    [[nodiscard]] static constexpr Instruction load(
        int dst_register,
        int address_register,
        int offset = 0) noexcept
    {
        return {
            Opcode::Load,
            dst_register,
            address_register,
            -1,
            offset,
        };
    }

    [[nodiscard]] static constexpr Instruction store(
        int address_register,
        int value_register,
        int offset = 0) noexcept
    {
        return {
            Opcode::Store,
            -1,
            address_register,
            value_register,
            offset,
        };
    }

    [[nodiscard]] static constexpr Instruction exit() noexcept
    {
        return {Opcode::Exit, -1, -1, -1, 0};
    }
};

struct ThreadContext {
    RegisterFile registers{};
    std::size_t pc{0};
    bool finished{false};

    bool operator==(const ThreadContext&) const = default;
};

struct LaneContext {
    RegisterFile registers{};
    bool finished{false};

    bool operator==(const LaneContext&) const = default;
};

struct Warp {
    std::array<LaneContext, kWarpLaneCount> lanes{};
    ActiveMask active_mask{0b1111};
    std::size_t pc{0};
    bool finished{false};

    bool operator==(const Warp&) const = default;
};

using Program = std::vector<Instruction>;

[[nodiscard]] StepResult step(
    const Program& program,
    ThreadContext& context) noexcept;

[[nodiscard]] StepResult step(
    const Program& program,
    ThreadContext& context,
    GlobalMemory& memory) noexcept;

[[nodiscard]] StepResult step(
    const Program& program,
    Warp& warp) noexcept;

[[nodiscard]] StepResult step(
    const Program& program,
    Warp& warp,
    GlobalMemory& memory) noexcept;

[[nodiscard]] std::string opcode_name(Opcode opcode);
[[nodiscard]] std::string step_result_name(StepResult result);
[[nodiscard]] std::string format_instruction(const Instruction& instruction);
[[nodiscard]] std::string format_context(const ThreadContext& context);
[[nodiscard]] std::string format_warp(const Warp& warp);

} // namespace minisimt
