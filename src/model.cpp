#include "minisimt/model.hpp"

#include <cstdint>
#include <sstream>
#include <utility>

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

[[nodiscard]] bool has_no_control_operands(
    const Instruction& instruction) noexcept
{
    return instruction.target == -1 &&
           instruction.reconverge == -1;
}

[[nodiscard]] bool is_valid_instruction(
    const Instruction& instruction) noexcept
{
    switch (instruction.opcode) {
    case Opcode::MovImm:
        return is_valid_register(instruction.dst) &&
               instruction.src1 == -1 &&
               instruction.src2 == -1 &&
               has_no_control_operands(instruction);

    case Opcode::MovReg:
        return is_valid_register(instruction.dst) &&
               is_valid_register(instruction.src1) &&
               instruction.src2 == -1 &&
               instruction.immediate == 0 &&
               has_no_control_operands(instruction);

    case Opcode::Add:
    case Opcode::Mul:
    case Opcode::CmpLt:
        return is_valid_register(instruction.dst) &&
               is_valid_register(instruction.src1) &&
               is_valid_register(instruction.src2) &&
               instruction.immediate == 0 &&
               has_no_control_operands(instruction);

    case Opcode::LaneId:
        return is_valid_register(instruction.dst) &&
               instruction.src1 == -1 &&
               instruction.src2 == -1 &&
               instruction.immediate == 0 &&
               has_no_control_operands(instruction);

    case Opcode::Load:
        return is_valid_register(instruction.dst) &&
               is_valid_register(instruction.src1) &&
               instruction.src2 == -1 &&
               has_no_control_operands(instruction);

    case Opcode::Store:
        return instruction.dst == -1 &&
               is_valid_register(instruction.src1) &&
               is_valid_register(instruction.src2) &&
               has_no_control_operands(instruction);

    case Opcode::BranchIf:
        return instruction.dst == -1 &&
               is_valid_register(instruction.src1) &&
               instruction.src2 == -1 &&
               instruction.immediate == 0 &&
               instruction.target >= 0 &&
               instruction.reconverge >= 0;

    case Opcode::Jump:
        return has_no_register_operands(instruction) &&
               instruction.immediate == 0 &&
               instruction.target >= 0 &&
               instruction.reconverge == -1;

    case Opcode::Exit:
        return has_no_register_operands(instruction) &&
               instruction.immediate == 0 &&
               has_no_control_operands(instruction);
    }

    return false;
}

[[nodiscard]] bool has_valid_control_flow(
    const Instruction& instruction,
    std::size_t pc,
    const Program& program) noexcept
{
    switch (instruction.opcode) {
    case Opcode::BranchIf: {
        const auto target =
            static_cast<std::size_t>(instruction.target);
        const auto reconverge =
            static_cast<std::size_t>(instruction.reconverge);
        if (target >= program.size() ||
            reconverge >= program.size() ||
            target <= pc + 1 ||
            reconverge <= target) {
            return false;
        }

        const Instruction& fallthrough_delimiter =
            program[target - 1];
        return fallthrough_delimiter.opcode == Opcode::Jump &&
               is_valid_instruction(fallthrough_delimiter) &&
               static_cast<std::size_t>(
                   fallthrough_delimiter.target) == reconverge;
    }

    case Opcode::Jump: {
        const auto target =
            static_cast<std::size_t>(instruction.target);
        return target < program.size() && target > pc;
    }

    default:
        return true;
    }
}

[[nodiscard]] bool is_memory_instruction(Opcode opcode) noexcept
{
    return opcode == Opcode::Load || opcode == Opcode::Store;
}

[[nodiscard]] bool resolve_address(
    const Instruction& instruction,
    const RegisterFile& registers,
    std::size_t memory_size,
    std::size_t& address) noexcept
{
    const auto base = static_cast<std::int64_t>(
        registers[static_cast<std::size_t>(instruction.src1)]);
    const auto offset = static_cast<std::int64_t>(instruction.immediate);
    const std::int64_t resolved = base + offset;

    if (resolved < 0 ||
        static_cast<std::uint64_t>(resolved) >=
            static_cast<std::uint64_t>(memory_size)) {
        return false;
    }

    address = static_cast<std::size_t>(resolved);
    return true;
}

[[nodiscard]] bool has_valid_memory_access(
    const Instruction& instruction,
    const RegisterFile& registers,
    const GlobalMemory* memory) noexcept
{
    if (!is_memory_instruction(instruction.opcode)) {
        return true;
    }

    if (memory == nullptr) {
        return false;
    }

    std::size_t address = 0;
    return resolve_address(
        instruction,
        registers,
        memory->size(),
        address);
}

void execute_lane_instruction(
    const Instruction& instruction,
    RegisterFile& registers,
    std::size_t lane_id,
    GlobalMemory* memory) noexcept
{
    switch (instruction.opcode) {
    case Opcode::MovImm:
        registers[static_cast<std::size_t>(instruction.dst)] =
            instruction.immediate;
        return;

    case Opcode::MovReg:
        registers[static_cast<std::size_t>(instruction.dst)] =
            registers[static_cast<std::size_t>(instruction.src1)];
        return;

    case Opcode::Add: {
        const int value =
            registers[static_cast<std::size_t>(instruction.src1)] +
            registers[static_cast<std::size_t>(instruction.src2)];
        registers[static_cast<std::size_t>(instruction.dst)] = value;
        return;
    }

    case Opcode::Mul: {
        const int value =
            registers[static_cast<std::size_t>(instruction.src1)] *
            registers[static_cast<std::size_t>(instruction.src2)];
        registers[static_cast<std::size_t>(instruction.dst)] = value;
        return;
    }

    case Opcode::CmpLt:
        registers[static_cast<std::size_t>(instruction.dst)] =
            registers[static_cast<std::size_t>(instruction.src1)] <
                    registers[static_cast<std::size_t>(instruction.src2)]
                ? 1
                : 0;
        return;

    case Opcode::LaneId:
        registers[static_cast<std::size_t>(instruction.dst)] =
            static_cast<int>(lane_id);
        return;

    case Opcode::Load: {
        std::size_t address = 0;
        static_cast<void>(resolve_address(
            instruction,
            registers,
            memory->size(),
            address));
        registers[static_cast<std::size_t>(instruction.dst)] =
            (*memory)[address];
        return;
    }

    case Opcode::Store: {
        std::size_t address = 0;
        static_cast<void>(resolve_address(
            instruction,
            registers,
            memory->size(),
            address));
        (*memory)[address] =
            registers[static_cast<std::size_t>(instruction.src2)];
        return;
    }

    case Opcode::BranchIf:
    case Opcode::Jump:
    case Opcode::Exit:
        return;
    }
}

void advance_reconvergence(Warp& warp) noexcept
{
    while (!warp.reconvergence_stack.empty()) {
        ReconvergenceFrame& frame =
            warp.reconvergence_stack.back();
        if (warp.pc != frame.reconverge_pc) {
            return;
        }

        if (!frame.pending_path_started) {
            frame.pending_path_started = true;
            warp.active_mask = frame.pending_mask;
            warp.pc = frame.pending_pc;
            return;
        }

        warp.active_mask = frame.reconverge_mask;
        warp.reconvergence_stack.pop_back();
    }
}

StepResult step_scalar(
    const Program& program,
    ThreadContext& context,
    GlobalMemory* memory) noexcept
{
    if (context.finished) {
        return StepResult::Halted;
    }

    if (context.pc >= program.size()) {
        return StepResult::Error;
    }

    const Instruction& instruction = program[context.pc];
    if (!is_valid_instruction(instruction)) {
        return StepResult::Error;
    }

    if (!has_valid_control_flow(
            instruction,
            context.pc,
            program)) {
        return StepResult::Error;
    }

    if (!has_valid_memory_access(
            instruction,
            context.registers,
            memory)) {
        return StepResult::Error;
    }

    if (instruction.opcode == Opcode::Exit) {
        context.finished = true;
        return StepResult::Halted;
    }

    if (instruction.opcode == Opcode::BranchIf) {
        if (context.registers[
                static_cast<std::size_t>(instruction.src1)] != 0) {
            context.pc =
                static_cast<std::size_t>(instruction.target);
        } else {
            ++context.pc;
        }
        return StepResult::Executed;
    }

    if (instruction.opcode == Opcode::Jump) {
        context.pc = static_cast<std::size_t>(instruction.target);
        return StepResult::Executed;
    }

    execute_lane_instruction(
        instruction,
        context.registers,
        0,
        memory);
    ++context.pc;
    return StepResult::Executed;
}

StepResult step_warp(
    const Program& program,
    Warp& warp,
    GlobalMemory* memory)
{
    if (warp.finished || warp.active_mask.none()) {
        return StepResult::Halted;
    }

    if (warp.pc >= program.size()) {
        return StepResult::Error;
    }

    const Instruction& instruction = program[warp.pc];
    if (!is_valid_instruction(instruction)) {
        return StepResult::Error;
    }

    if (!has_valid_control_flow(
            instruction,
            warp.pc,
            program)) {
        return StepResult::Error;
    }

    if (!warp.reconvergence_stack.empty() &&
        (instruction.opcode == Opcode::BranchIf ||
         instruction.opcode == Opcode::Exit)) {
        return StepResult::Error;
    }

    if (!warp.reconvergence_stack.empty() &&
        instruction.opcode == Opcode::Jump &&
        static_cast<std::size_t>(instruction.target) !=
            warp.reconvergence_stack.back().reconverge_pc) {
        return StepResult::Error;
    }

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        if (warp.active_mask.test(lane) && warp.lanes[lane].finished) {
            return StepResult::Error;
        }
    }

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        if (warp.active_mask.test(lane) &&
            !has_valid_memory_access(
                instruction,
                warp.lanes[lane].registers,
                memory)) {
            return StepResult::Error;
        }
    }

    Warp next = warp;

    if (instruction.opcode == Opcode::Exit) {
        for (std::size_t lane = 0; lane < next.lanes.size(); ++lane) {
            if (next.active_mask.test(lane)) {
                next.lanes[lane].finished = true;
            }
        }

        next.active_mask.reset();
        next.finished = true;
        warp = std::move(next);
        return StepResult::Halted;
    }

    if (instruction.opcode == Opcode::BranchIf) {
        ActiveMask taken_mask;
        for (std::size_t lane = 0; lane < next.lanes.size(); ++lane) {
            if (next.active_mask.test(lane) &&
                next.lanes[lane].registers[
                    static_cast<std::size_t>(instruction.src1)] != 0) {
                taken_mask.set(lane);
            }
        }

        const ActiveMask fallthrough_mask =
            next.active_mask & ~taken_mask;
        if (taken_mask.none()) {
            ++next.pc;
        } else if (fallthrough_mask.none()) {
            next.pc = static_cast<std::size_t>(instruction.target);
        } else {
            next.reconvergence_stack.push_back({
                next.active_mask,
                taken_mask,
                static_cast<std::size_t>(instruction.target),
                static_cast<std::size_t>(instruction.reconverge),
                false,
            });
            next.active_mask = fallthrough_mask;
            ++next.pc;
        }

        advance_reconvergence(next);
        warp = std::move(next);
        return StepResult::Executed;
    }

    if (instruction.opcode == Opcode::Jump) {
        next.pc = static_cast<std::size_t>(instruction.target);
        advance_reconvergence(next);
        warp = std::move(next);
        return StepResult::Executed;
    }

    for (std::size_t lane = 0; lane < next.lanes.size(); ++lane) {
        if (next.active_mask.test(lane)) {
            execute_lane_instruction(
                instruction,
                next.lanes[lane].registers,
                lane,
                memory);
        }
    }

    ++next.pc;
    advance_reconvergence(next);
    warp = std::move(next);
    return StepResult::Executed;
}

} // namespace

StepResult step(const Program& program, ThreadContext& context) noexcept
{
    return step_scalar(program, context, nullptr);
}

StepResult step(
    const Program& program,
    ThreadContext& context,
    GlobalMemory& memory) noexcept
{
    return step_scalar(program, context, &memory);
}

StepResult step(const Program& program, Warp& warp)
{
    return step_warp(program, warp, nullptr);
}

StepResult step(
    const Program& program,
    Warp& warp,
    GlobalMemory& memory)
{
    return step_warp(program, warp, &memory);
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
    case Opcode::Mul:
        return "MUL";
    case Opcode::CmpLt:
        return "CMP_LT";
    case Opcode::LaneId:
        return "LANE_ID";
    case Opcode::Load:
        return "LD";
    case Opcode::Store:
        return "ST";
    case Opcode::BranchIf:
        return "BRA_IF";
    case Opcode::Jump:
        return "JUMP";
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
    case Opcode::Mul:
        output << " R" << instruction.dst
               << ", R" << instruction.src1
               << ", R" << instruction.src2;
        break;
    case Opcode::CmpLt:
        output << " R" << instruction.dst
               << ", R" << instruction.src1
               << ", R" << instruction.src2;
        break;
    case Opcode::LaneId:
        output << " R" << instruction.dst;
        break;
    case Opcode::Load:
        output << " R" << instruction.dst
               << ", [R" << instruction.src1;
        if (instruction.immediate > 0) {
            output << " + " << instruction.immediate;
        } else if (instruction.immediate < 0) {
            output << " - "
                   << -static_cast<std::int64_t>(instruction.immediate);
        }
        output << ']';
        break;
    case Opcode::Store:
        output << " [R" << instruction.src1;
        if (instruction.immediate > 0) {
            output << " + " << instruction.immediate;
        } else if (instruction.immediate < 0) {
            output << " - "
                   << -static_cast<std::int64_t>(instruction.immediate);
        }
        output << "], R" << instruction.src2;
        break;
    case Opcode::BranchIf:
        output << " R" << instruction.src1
               << ", " << instruction.target
               << ", reconverge=" << instruction.reconverge;
        break;
    case Opcode::Jump:
        output << ' ' << instruction.target;
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

std::string format_warp(const Warp& warp)
{
    std::ostringstream output;
    output << "pc=" << warp.pc
           << " finished=" << (warp.finished ? "true" : "false")
           << " active_mask=0b" << warp.active_mask.to_string()
           << " reconvergence_depth="
           << warp.reconvergence_stack.size();

    for (std::size_t index = 0;
         index < warp.reconvergence_stack.size();
         ++index) {
        const ReconvergenceFrame& frame =
            warp.reconvergence_stack[index];
        output << "\n  frame=" << index
               << " reconverge_mask=0b"
               << frame.reconverge_mask.to_string()
               << " pending_mask=0b"
               << frame.pending_mask.to_string()
               << " pending_pc=" << frame.pending_pc
               << " reconverge_pc=" << frame.reconverge_pc
               << " pending_path_started="
               << (frame.pending_path_started ? "true" : "false");
    }

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const LaneContext& context = warp.lanes[lane];
        output << "\n  lane=" << lane
               << " active="
               << (warp.active_mask.test(lane) ? "true" : "false")
               << " finished="
               << (context.finished ? "true" : "false");

        for (std::size_t index = 0;
             index < context.registers.size();
             ++index) {
            output << " R" << index << '=' << context.registers[index];
        }
    }

    return output.str();
}

} // namespace minisimt
