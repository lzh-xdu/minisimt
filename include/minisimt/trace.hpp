#pragma once

#include "minisimt/model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace minisimt {

enum class MemoryAccessKind {
    Load,
    Store,
};

struct MemoryAccessRecord {
    std::size_t lane{0};
    MemoryAccessKind kind{MemoryAccessKind::Load};
    std::size_t address{0};
    int value{0};

    bool operator==(const MemoryAccessRecord&) const = default;
};

struct WarpTraceRecord {
    Warp before{};
    std::optional<Instruction> instruction{};
    StepResult result{StepResult::Error};
    Warp after{};
    std::vector<MemoryAccessRecord> memory_accesses{};

    bool operator==(const WarpTraceRecord&) const = default;
};

[[nodiscard]] StepResult step_with_trace(
    const Program& program,
    Warp& warp,
    WarpTraceRecord& record);

[[nodiscard]] StepResult step_with_trace(
    const Program& program,
    Warp& warp,
    GlobalMemory& memory,
    WarpTraceRecord& record);

[[nodiscard]] std::string memory_access_kind_name(MemoryAccessKind kind);

[[nodiscard]] std::string format_trace_json(
    const WarpTraceRecord& record);

} // namespace minisimt
