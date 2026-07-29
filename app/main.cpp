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

} // namespace

int main(int argc, char* argv[])
{
    const bool emit_trace =
        argc == 2 && std::string_view{argv[1]} == "--trace";
    if (argc > 2 ||
        (argc == 2 && !emit_trace)) {
        std::cerr << "Usage: minisimt_demo [--trace]\n";
        return 2;
    }

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
