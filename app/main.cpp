#include "minisimt/trace.hpp"

#include <array>
#include <iostream>
#include <string_view>

namespace {

template <std::size_t Size>
void print_values(
    std::string_view name,
    const std::array<int, Size>& values)
{
    std::cout << name << " = [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ", ";
        }
        std::cout << values[index];
    }
    std::cout << "]\n";
}

int run_vector_add(bool emit_trace)
{
    constexpr std::size_t vector_size = minisimt::kWarpLaneCount;
    constexpr int a_base = 0;
    constexpr int b_base = static_cast<int>(vector_size);
    constexpr int c_base = static_cast<int>(vector_size * 2);

    const minisimt::Program program{
        minisimt::Instruction::lane_id(0),
        minisimt::Instruction::load(1, 0, a_base),
        minisimt::Instruction::load(2, 0, b_base),
        minisimt::Instruction::add(3, 1, 2),
        minisimt::Instruction::store(0, 3, c_base),
        minisimt::Instruction::exit(),
    };

    const std::array<int, vector_size> a{1, 2, 3, 4};
    const std::array<int, vector_size> b{10, 20, 30, 40};
    const std::array<int, vector_size> expected{11, 22, 33, 44};

    minisimt::GlobalMemory memory(vector_size * 3, 0);
    for (std::size_t index = 0; index < vector_size; ++index) {
        memory[static_cast<std::size_t>(a_base) + index] = a[index];
        memory[static_cast<std::size_t>(b_base) + index] = b[index];
    }

    minisimt::Warp warp;
    while (true) {
        minisimt::WarpTraceRecord record;
        const minisimt::StepResult result =
            minisimt::step_with_trace(program, warp, memory, record);

        if (emit_trace) {
            std::cout << minisimt::format_trace_json(record) << '\n';
        }

        if (result != minisimt::StepResult::Executed) {
            if (result == minisimt::StepResult::Error) {
                std::cerr << "MiniSIMT execution failed at pc="
                          << warp.pc << '\n';
                return 1;
            }
            break;
        }
    }

    std::array<int, vector_size> c{};
    for (std::size_t index = 0; index < vector_size; ++index) {
        c[index] = memory[static_cast<std::size_t>(c_base) + index];
    }

    if (c != expected) {
        std::cerr << "Vector-add result mismatch\n";
        return 1;
    }

    if (!emit_trace) {
        std::cout << "MiniSIMT four-lane vector-add kernel\n";
        print_values("A", a);
        print_values("B", b);
        print_values("C", c);
        std::cout << "status = PASS\n";
    }

    return 0;
}

int run_branch_divergence(bool emit_trace)
{
    const minisimt::Program program{
        minisimt::Instruction::lane_id(0),
        minisimt::Instruction::mov_imm(1, 2),
        minisimt::Instruction::cmp_lt(2, 0, 1),
        minisimt::Instruction::branch_if(2, 6, 7),
        minisimt::Instruction::mov_imm(3, 200),
        minisimt::Instruction::jump(7),
        minisimt::Instruction::mov_imm(3, 100),
        minisimt::Instruction::add(4, 3, 0),
        minisimt::Instruction::exit(),
    };

    minisimt::Warp warp;
    while (true) {
        minisimt::WarpTraceRecord record;
        const minisimt::StepResult result =
            minisimt::step_with_trace(program, warp, record);

        if (emit_trace) {
            std::cout << minisimt::format_trace_json(record) << '\n';
        }

        if (result != minisimt::StepResult::Executed) {
            if (result == minisimt::StepResult::Error) {
                std::cerr << "MiniSIMT branch execution failed at pc="
                          << warp.pc << '\n';
                return 1;
            }
            break;
        }
    }

    std::array<int, minisimt::kWarpLaneCount> predicate{};
    std::array<int, minisimt::kWarpLaneCount> path_value{};
    std::array<int, minisimt::kWarpLaneCount> merged{};
    for (std::size_t lane = 0; lane < minisimt::kWarpLaneCount; ++lane) {
        predicate[lane] = warp.lanes[lane].registers[2];
        path_value[lane] = warp.lanes[lane].registers[3];
        merged[lane] = warp.lanes[lane].registers[4];
    }

    constexpr std::array<int, minisimt::kWarpLaneCount>
        expected_predicate{1, 1, 0, 0};
    constexpr std::array<int, minisimt::kWarpLaneCount>
        expected_path_value{100, 100, 200, 200};
    constexpr std::array<int, minisimt::kWarpLaneCount>
        expected_merged{100, 101, 202, 203};

    if (predicate != expected_predicate ||
        path_value != expected_path_value ||
        merged != expected_merged) {
        std::cerr << "Branch-divergence result mismatch\n";
        return 1;
    }

    if (!emit_trace) {
        std::cout << "MiniSIMT four-lane branch divergence\n";
        print_values("predicate", predicate);
        print_values("path_value", path_value);
        print_values("merged", merged);
        std::cout << "status = PASS\n";
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    bool emit_trace = false;
    bool branch_demo = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--trace" && !emit_trace) {
            emit_trace = true;
        } else if (argument == "--branch" && !branch_demo) {
            branch_demo = true;
        } else {
            std::cerr << "Usage: minisimt_demo [--branch] [--trace]\n";
            return 2;
        }
    }

    return branch_demo
        ? run_branch_divergence(emit_trace)
        : run_vector_add(emit_trace);
}
