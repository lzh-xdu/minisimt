#include "minisimt/model.hpp"

#include <sstream>

namespace minisimt {
namespace {

[[nodiscard]] bool is_valid_register(int index) noexcept
{
    return index >= 0 &&
           static_cast<std::size_t>(index) < kRegisterCount;
}

[[nodiscard]] bool has_no_register_operands(
    const Instruction& instruction) noexcept
{
    return instruction.dst == -1 &&
           instruction.src1 == -1 &&
           instruction.src2 == -1;
}

} // namespace

StepResult step(const Program& program, ThreadContext& context) noexcept
{
    if (context.finished) {
        return StepResult::Halted;
    }

    if (context.pc >= program.size()) {
        return StepResult::Error;
    }

    const Instruction& instruction = program[context.pc];

    switch (instruction.opcode) {
    case Opcode::MovImm:
        if (!is_valid_register(instruction.dst) ||
            instruction.src1 != -1 ||
            instruction.src2 != -1) {
            return StepResult::Error;
        }

        context.registers[static_cast<std::size_t>(instruction.dst)] =
            instruction.immediate;
        ++context.pc;
        return StepResult::Executed;

    case Opcode::MovReg:
        if (!is_valid_register(instruction.dst) ||
            !is_valid_register(instruction.src1) ||
            instruction.src2 != -1 ||
            instruction.immediate != 0) {
            return StepResult::Error;
        }

        context.registers[static_cast<std::size_t>(instruction.dst)] =
            context.registers[static_cast<std::size_t>(instruction.src1)];
        ++context.pc;
        return StepResult::Executed;

    case Opcode::Add: {
        if (!is_valid_register(instruction.dst) ||
            !is_valid_register(instruction.src1) ||
            !is_valid_register(instruction.src2) ||
            instruction.immediate != 0) {
            return StepResult::Error;
        }

        const int value =
            context.registers[static_cast<std::size_t>(instruction.src1)] +
            context.registers[static_cast<std::size_t>(instruction.src2)];
        context.registers[static_cast<std::size_t>(instruction.dst)] = value;
        ++context.pc;
        return StepResult::Executed;
    }

    case Opcode::Exit:
        if (!has_no_register_operands(instruction) ||
            instruction.immediate != 0) {
            return StepResult::Error;
        }

        context.finished = true;
        return StepResult::Halted;
    }

    return StepResult::Error;
}

std::string opcode_name(Opcode opcode)
{
    switch (opcode) {
    case Opcode::MovImm:
        return "MOV_IMM";
    case Opcode::MovReg:
        return "MOV_REG";
    case Opcode::Add:
        return "ADD";
    case Opcode::Exit:
        return "EXIT";
    }

    return "UNKNOWN";
}

std::string step_result_name(StepResult result)
{
    switch (result) {
    case StepResult::Executed:
        return "Executed";
    case StepResult::Halted:
        return "Halted";
    case StepResult::Error:
        return "Error";
    }

    return "Unknown";
}

std::string format_instruction(const Instruction& instruction)
{
    std::ostringstream output;
    output << opcode_name(instruction.opcode);

    switch (instruction.opcode) {
    case Opcode::MovImm:
        output << " R" << instruction.dst
               << ", " << instruction.immediate;
        break;
    case Opcode::MovReg:
        output << " R" << instruction.dst
               << ", R" << instruction.src1;
        break;
    case Opcode::Add:
        output << " R" << instruction.dst
               << ", R" << instruction.src1
               << ", R" << instruction.src2;
        break;
    case Opcode::Exit:
        break;
    }

    return output.str();
}

std::string format_context(const ThreadContext& context)
{
    std::ostringstream output;
    output << "pc=" << context.pc
           << " finished=" << (context.finished ? "true" : "false");

    for (std::size_t index = 0; index < context.registers.size(); ++index) {
        output << " R" << index << '=' << context.registers[index];
    }

    return output.str();
}

} // namespace minisimt
