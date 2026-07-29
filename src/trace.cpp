#include "minisimt/trace.hpp"

#include <cstdint>
#include <sstream>

namespace minisimt {
namespace {

[[nodiscard]] std::size_t resolve_trace_address(
    const Instruction& instruction,
    const RegisterFile& registers) noexcept
{
    const auto base = static_cast<std::int64_t>(
        registers[static_cast<std::size_t>(instruction.src1)]);
    const auto offset = static_cast<std::int64_t>(instruction.immediate);
    return static_cast<std::size_t>(base + offset);
}

void append_instruction_json(
    std::ostringstream& output,
    const std::optional<Instruction>& instruction)
{
    if (!instruction.has_value()) {
        output << "null";
        return;
    }

    output << "{\"opcode\":\""
           << opcode_name(instruction->opcode)
           << "\",\"dst\":" << instruction->dst
           << ",\"src1\":" << instruction->src1
           << ",\"src2\":" << instruction->src2
           << ",\"immediate\":" << instruction->immediate
           << '}';
}

void append_warp_json(
    std::ostringstream& output,
    const Warp& warp)
{
    output << "{\"pc\":" << warp.pc
           << ",\"finished\":"
           << (warp.finished ? "true" : "false")
           << ",\"active_mask\":\""
           << warp.active_mask.to_string()
           << "\",\"lanes\":[";

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        if (lane != 0) {
            output << ',';
        }

        const LaneContext& context = warp.lanes[lane];
        output << "{\"finished\":"
               << (context.finished ? "true" : "false")
               << ",\"registers\":[";

        for (std::size_t index = 0;
             index < context.registers.size();
             ++index) {
            if (index != 0) {
                output << ',';
            }
            output << context.registers[index];
        }

        output << "]}";
    }

    output << "]}";
}

void append_memory_accesses_json(
    std::ostringstream& output,
    const std::vector<MemoryAccessRecord>& accesses)
{
    output << '[';

    for (std::size_t index = 0; index < accesses.size(); ++index) {
        if (index != 0) {
            output << ',';
        }

        const MemoryAccessRecord& access = accesses[index];
        output << "{\"lane\":" << access.lane
               << ",\"kind\":\""
               << memory_access_kind_name(access.kind)
               << "\",\"address\":" << access.address
               << ",\"value\":" << access.value
               << '}';
    }

    output << ']';
}

void collect_memory_accesses(
    const WarpTraceRecord& record,
    const GlobalMemory& memory,
    std::vector<MemoryAccessRecord>& accesses)
{
    if (record.result != StepResult::Executed ||
        !record.instruction.has_value()) {
        return;
    }

    const Instruction& instruction = record.instruction.value();
    if (instruction.opcode != Opcode::Load &&
        instruction.opcode != Opcode::Store) {
        return;
    }

    for (std::size_t lane = 0;
         lane < record.before.lanes.size();
         ++lane) {
        if (!record.before.active_mask.test(lane)) {
            continue;
        }

        const RegisterFile& registers =
            record.before.lanes[lane].registers;
        const std::size_t address =
            resolve_trace_address(instruction, registers);
        const int value =
            instruction.opcode == Opcode::Load
                ? memory[address]
                : registers[static_cast<std::size_t>(instruction.src2)];

        accesses.push_back({
            lane,
            instruction.opcode == Opcode::Load
                ? MemoryAccessKind::Load
                : MemoryAccessKind::Store,
            address,
            value,
        });
    }
}

} // namespace

StepResult step_with_trace(
    const Program& program,
    Warp& warp,
    WarpTraceRecord& record)
{
    record = WarpTraceRecord{};
    record.before = warp;

    if (!warp.finished &&
        warp.active_mask.any() &&
        warp.pc < program.size()) {
        record.instruction = program[warp.pc];
    }

    record.result = step(program, warp);
    record.after = warp;
    return record.result;
}

StepResult step_with_trace(
    const Program& program,
    Warp& warp,
    GlobalMemory& memory,
    WarpTraceRecord& record)
{
    record = WarpTraceRecord{};
    record.before = warp;

    if (!warp.finished &&
        warp.active_mask.any() &&
        warp.pc < program.size()) {
        record.instruction = program[warp.pc];
    }

    record.result = step(program, warp, memory);
    record.after = warp;
    collect_memory_accesses(record, memory, record.memory_accesses);
    return record.result;
}

std::string memory_access_kind_name(MemoryAccessKind kind)
{
    switch (kind) {
    case MemoryAccessKind::Load:
        return "load";
    case MemoryAccessKind::Store:
        return "store";
    }

    return "unknown";
}

std::string format_trace_json(const WarpTraceRecord& record)
{
    std::ostringstream output;
    output << "{\"result\":\""
           << step_result_name(record.result)
           << "\",\"instruction\":";
    append_instruction_json(output, record.instruction);
    output << ",\"memory_accesses\":";
    append_memory_accesses_json(output, record.memory_accesses);
    output << ",\"before\":";
    append_warp_json(output, record.before);
    output << ",\"after\":";
    append_warp_json(output, record.after);
    output << '}';
    return output.str();
}

} // namespace minisimt
