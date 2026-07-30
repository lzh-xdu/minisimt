#include "minisimt/trace.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;
int assertions = 0;

void check(bool condition, std::string_view message)
{
    ++assertions;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

[[nodiscard]] minisimt::Program make_branch_program()
{
    return {
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
}

void test_mov_immediate()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(2, 30),
    };
    minisimt::ThreadContext context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "MOV_IMM returns Executed");
    check(context.registers[2] == 30, "MOV_IMM writes its immediate");
    check(context.pc == 1, "MOV_IMM advances the program counter");
    check(!context.finished, "MOV_IMM does not halt the thread");
}

void test_negative_immediate()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(0, -1),
    };
    minisimt::ThreadContext context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "negative immediate is valid");
    check(context.registers[0] == -1,
          "negative immediate is not confused with an unused operand");
}

void test_register_move()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_reg(2, 1),
    };
    minisimt::ThreadContext context;
    context.registers[1] = 42;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "MOV_REG returns Executed");
    check(context.registers[2] == 42,
          "MOV_REG copies its source register");
}

void test_add()
{
    const minisimt::Program program{
        minisimt::Instruction::add(2, 0, 1),
    };
    minisimt::ThreadContext context;
    context.registers[0] = 10;
    context.registers[1] = 20;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "ADD returns Executed");
    check(context.registers[2] == 30, "ADD writes the sum");
    check(context.pc == 1, "ADD advances the program counter");
}

void test_mul()
{
    const minisimt::Program program{
        minisimt::Instruction::mul(2, 0, 1),
    };
    minisimt::ThreadContext context;
    context.registers[0] = 6;
    context.registers[1] = -7;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "MUL returns Executed");
    check(context.registers[2] == -42, "MUL writes the product");
    check(context.pc == 1, "MUL advances the program counter");
}

void test_cmp_lt()
{
    const minisimt::Program program{
        minisimt::Instruction::cmp_lt(2, 0, 1),
    };
    minisimt::ThreadContext context;
    context.registers[0] = -2;
    context.registers[1] = 3;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Executed,
          "CMP_LT returns Executed");
    check(context.registers[2] == 1,
          "CMP_LT writes one when lhs is smaller");
    check(context.pc == 1,
          "CMP_LT advances the program counter");
}

void test_exit()
{
    const minisimt::Program program{
        minisimt::Instruction::exit(),
    };
    minisimt::ThreadContext context;

    const auto first_result = minisimt::step(program, context);
    const auto second_result = minisimt::step(program, context);

    check(first_result == minisimt::StepResult::Halted,
          "EXIT returns Halted");
    check(second_result == minisimt::StepResult::Halted,
          "a halted thread remains halted");
    check(context.finished, "EXIT marks the thread as finished");
    check(context.pc == 0, "EXIT keeps the program counter at EXIT");
}

void test_invalid_register_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(8, 30),
    };
    minisimt::ThreadContext context;
    context.registers[0] = 7;
    const minisimt::ThreadContext before = context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Error,
          "invalid destination returns Error");
    check(context == before,
          "invalid destination does not change architectural state");
}

void test_malformed_instruction_is_atomic()
{
    const minisimt::Program program{
        {minisimt::Opcode::MovImm, 0, 1, -1, 30},
    };
    minisimt::ThreadContext context;
    const minisimt::ThreadContext before = context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Error,
          "malformed MOV_IMM returns Error");
    check(context == before,
          "malformed MOV_IMM does not change architectural state");
}

void test_invalid_pc_is_atomic()
{
    const minisimt::Program program;
    minisimt::ThreadContext context;
    const minisimt::ThreadContext before = context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Error,
          "out-of-range pc returns Error");
    check(context == before,
          "out-of-range pc does not change architectural state");
}

void test_program_flow()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(0, 10),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::add(2, 0, 1),
        minisimt::Instruction::mov_reg(3, 2),
        minisimt::Instruction::exit(),
    };
    minisimt::ThreadContext context;

    minisimt::StepResult result = minisimt::StepResult::Executed;
    while (result == minisimt::StepResult::Executed) {
        result = minisimt::step(program, context);
    }

    check(result == minisimt::StepResult::Halted,
          "program terminates with EXIT");
    check(context.registers[0] == 10, "program keeps R0");
    check(context.registers[1] == 20, "program keeps R1");
    check(context.registers[2] == 30, "program computes R2");
    check(context.registers[3] == 30, "program copies R2 into R3");
    check(context.pc == 4, "program counter points at EXIT");
    check(context.finished, "program marks the thread as finished");
}

void test_scalar_branch_flow()
{
    const minisimt::Program program = make_branch_program();
    minisimt::ThreadContext context;

    minisimt::StepResult result = minisimt::StepResult::Executed;
    while (result == minisimt::StepResult::Executed) {
        result = minisimt::step(program, context);
    }

    check(result == minisimt::StepResult::Halted,
          "scalar branch program reaches EXIT");
    check(context.registers[2] == 1,
          "scalar CMP_LT produces a true predicate");
    check(context.registers[3] == 100,
          "scalar BRA_IF selects the taken path");
    check(context.registers[4] == 100,
          "scalar execution reaches the merge instruction");
    check(context.pc == 8,
          "scalar branch program leaves pc at EXIT");
}

void test_invalid_branch_target_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::branch_if(0, 3, 4),
    };
    minisimt::ThreadContext context;
    context.registers[0] = 1;
    const minisimt::ThreadContext thread_before = context;
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 1;
    const minisimt::Warp warp_before = warp;

    const auto thread_result = minisimt::step(program, context);
    const auto warp_result = minisimt::step(program, warp);

    check(thread_result == minisimt::StepResult::Error,
          "out-of-range scalar branch target returns Error");
    check(context == thread_before,
          "invalid scalar branch changes no state");
    check(warp_result == minisimt::StepResult::Error,
          "out-of-range warp branch target returns Error");
    check(warp == warp_before,
          "invalid warp branch changes no state");
}

void test_malformed_branch_layout_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::branch_if(0, 2, 3),
        minisimt::Instruction::mov_imm(1, 10),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::exit(),
    };
    minisimt::ThreadContext context;
    context.registers[0] = 1;
    const minisimt::ThreadContext thread_before = context;
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 1;
    const minisimt::Warp warp_before = warp;

    const auto thread_result = minisimt::step(program, context);
    const auto warp_result = minisimt::step(program, warp);

    check(thread_result == minisimt::StepResult::Error,
          "branch without a fallthrough delimiter returns scalar Error");
    check(context == thread_before,
          "malformed scalar branch layout changes no state");
    check(warp_result == minisimt::StepResult::Error,
          "branch without a fallthrough delimiter returns warp Error");
    check(warp == warp_before,
          "malformed warp branch layout changes no state");
}

void test_warp_executes_four_lanes()
{
    const minisimt::Program program{
        minisimt::Instruction::add(2, 0, 1),
    };
    minisimt::Warp warp;

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int lane_value = static_cast<int>(lane) + 1;
        warp.lanes[lane].registers[0] = lane_value;
        warp.lanes[lane].registers[1] = lane_value * 10;
    }

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Executed,
          "warp ADD returns Executed");
    check(warp.pc == 1, "warp ADD advances the shared pc once");
    check(warp.active_mask == minisimt::ActiveMask{0b1111},
          "warp ADD preserves the active mask");

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int expected = (static_cast<int>(lane) + 1) * 11;
        check(warp.lanes[lane].registers[2] == expected,
              "warp ADD computes each lane from lane-local registers");
    }
}

void test_warp_active_mask()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(2, 99),
    };
    minisimt::Warp warp;
    warp.active_mask = minisimt::ActiveMask{0b0101};

    for (auto& lane : warp.lanes) {
        lane.registers[2] = -1;
    }

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Executed,
          "masked warp instruction returns Executed");
    check(warp.lanes[0].registers[2] == 99,
          "active lane 0 executes");
    check(warp.lanes[1].registers[2] == -1,
          "inactive lane 1 remains unchanged");
    check(warp.lanes[2].registers[2] == 99,
          "active lane 2 executes");
    check(warp.lanes[3].registers[2] == -1,
          "inactive lane 3 remains unchanged");
    check(warp.pc == 1, "masked execution advances shared pc once");
}

void test_warp_mul()
{
    const minisimt::Program program{
        minisimt::Instruction::mul(2, 0, 1),
    };
    minisimt::Warp warp;

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int lane_value = static_cast<int>(lane) + 1;
        warp.lanes[lane].registers[0] = lane_value;
        warp.lanes[lane].registers[1] = lane_value + 4;
    }

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Executed,
          "warp MUL returns Executed");
    check(warp.pc == 1, "warp MUL advances shared pc once");

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int lane_value = static_cast<int>(lane) + 1;
        const int expected = lane_value * (lane_value + 4);
        check(warp.lanes[lane].registers[2] == expected,
              "warp MUL computes each lane product");
    }
}

void test_warp_invalid_instruction_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::add(8, 0, 1),
    };
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 10;
    warp.lanes[0].registers[1] = 20;
    const minisimt::Warp before = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Error,
          "invalid warp instruction returns Error");
    check(warp == before,
          "invalid warp instruction changes no lane or shared state");
}

void test_warp_invalid_pc_is_atomic()
{
    const minisimt::Program program;
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 7;
    const minisimt::Warp before = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Error,
          "out-of-range warp pc returns Error");
    check(warp == before,
          "out-of-range warp pc changes no architectural state");
}

void test_warp_rejects_active_finished_lane_atomically()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(0, 10),
    };
    minisimt::Warp warp;
    warp.lanes[2].finished = true;
    const minisimt::Warp before = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Error,
          "active finished lane is an invalid warp state");
    check(warp == before,
          "invalid active lane state is atomic");
}

void test_warp_empty_mask_is_halted()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(0, 10),
    };
    minisimt::Warp warp;
    warp.active_mask.reset();
    const minisimt::Warp before = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Halted,
          "a warp with no active lanes is halted");
    check(warp == before,
          "empty active mask does not change state");
}

void test_warp_exit()
{
    const minisimt::Program program{
        minisimt::Instruction::exit(),
    };
    minisimt::Warp warp;
    warp.active_mask = minisimt::ActiveMask{0b0111};

    const auto first_result = minisimt::step(program, warp);
    const minisimt::Warp after_exit = warp;
    const auto second_result = minisimt::step(program, warp);

    check(first_result == minisimt::StepResult::Halted,
          "warp EXIT returns Halted");
    check(second_result == minisimt::StepResult::Halted,
          "finished warp remains halted");
    check(warp == after_exit,
          "step after warp EXIT changes no state");
    check(warp.finished, "warp EXIT marks the warp finished");
    check(warp.active_mask.none(),
          "warp EXIT clears the active mask");
    check(warp.pc == 0, "warp EXIT keeps pc at EXIT");
    check(warp.lanes[0].finished &&
              warp.lanes[1].finished &&
              warp.lanes[2].finished,
          "warp EXIT finishes all active lanes");
    check(!warp.lanes[3].finished,
          "warp EXIT does not finish an inactive lane");
}

void test_warp_program_flow()
{
    const minisimt::Program program{
        minisimt::Instruction::add(2, 0, 1),
        minisimt::Instruction::mov_reg(3, 2),
        minisimt::Instruction::exit(),
    };
    minisimt::Warp warp;

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int lane_value = static_cast<int>(lane) + 1;
        warp.lanes[lane].registers[0] = lane_value;
        warp.lanes[lane].registers[1] = lane_value * 10;
    }

    minisimt::StepResult result = minisimt::StepResult::Executed;
    while (result == minisimt::StepResult::Executed) {
        result = minisimt::step(program, warp);
    }

    check(result == minisimt::StepResult::Halted,
          "warp program terminates with EXIT");
    check(warp.pc == 2, "warp pc points at EXIT");
    check(warp.finished, "warp program marks warp finished");
    check(warp.active_mask.none(),
          "finished warp has no active lanes");

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        const int expected = (static_cast<int>(lane) + 1) * 11;
        check(warp.lanes[lane].registers[2] == expected,
              "warp program computes lane-local ADD result");
        check(warp.lanes[lane].registers[3] == expected,
              "warp program copies each lane result");
        check(warp.lanes[lane].finished,
              "warp program finishes each active lane");
    }
}

void test_warp_uniform_branch_avoids_stack()
{
    const minisimt::Program program{
        minisimt::Instruction::branch_if(0, 3, 4),
        minisimt::Instruction::mov_imm(1, 10),
        minisimt::Instruction::jump(4),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::exit(),
    };
    minisimt::Warp all_taken;
    minisimt::Warp none_taken;
    for (auto& lane : all_taken.lanes) {
        lane.registers[0] = 1;
    }

    const auto taken_result = minisimt::step(program, all_taken);
    const auto fallthrough_result =
        minisimt::step(program, none_taken);

    check(taken_result == minisimt::StepResult::Executed,
          "uniform taken BRA_IF returns Executed");
    check(all_taken.pc == 3,
          "uniform taken BRA_IF jumps to its target");
    check(all_taken.active_mask == minisimt::ActiveMask{0b1111},
          "uniform taken BRA_IF preserves the active mask");
    check(all_taken.reconvergence_stack.empty(),
          "uniform taken BRA_IF creates no stack frame");
    check(fallthrough_result == minisimt::StepResult::Executed,
          "uniform fallthrough BRA_IF returns Executed");
    check(none_taken.pc == 1,
          "uniform fallthrough BRA_IF advances to the next instruction");
    check(none_taken.active_mask == minisimt::ActiveMask{0b1111},
          "uniform fallthrough BRA_IF preserves the active mask");
    check(none_taken.reconvergence_stack.empty(),
          "uniform fallthrough BRA_IF creates no stack frame");
}

void test_warp_branch_divergence_and_reconvergence()
{
    const minisimt::Program program = make_branch_program();
    minisimt::Warp warp;

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "branch demo executes LANE_ID");
    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "branch demo initializes split threshold");
    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "branch demo computes per-lane predicates");

    const auto branch_result = minisimt::step(program, warp);

    check(branch_result == minisimt::StepResult::Executed,
          "divergent BRA_IF returns Executed");
    check(warp.pc == 4,
          "divergent BRA_IF executes the fallthrough path first");
    check(warp.active_mask == minisimt::ActiveMask{0b1100},
          "fallthrough path activates lanes 2 and 3");
    check(warp.reconvergence_stack.size() == 1,
          "divergent BRA_IF pushes one reconvergence frame");
    check(
        warp.reconvergence_stack.back().pending_mask ==
            minisimt::ActiveMask{0b0011},
        "reconvergence frame defers taken lanes 0 and 1");
    check(!warp.reconvergence_stack.back().pending_path_started,
          "taken path is initially pending");

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "fallthrough path writes its path value");
    check(warp.pc == 5 &&
              warp.active_mask == minisimt::ActiveMask{0b1100},
          "fallthrough path remains active before JUMP");

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "fallthrough JUMP reaches the reconvergence point");
    check(warp.pc == 6,
          "reconvergence stack redirects execution to the taken path");
    check(warp.active_mask == minisimt::ActiveMask{0b0011},
          "taken path activates lanes 0 and 1");
    check(warp.reconvergence_stack.back().pending_path_started,
          "reconvergence frame records the active pending path");

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "taken path writes its path value");
    check(warp.pc == 7,
          "taken path reaches the merge instruction");
    check(warp.active_mask == minisimt::ActiveMask{0b1111},
          "merge restores the original active mask");
    check(warp.reconvergence_stack.empty(),
          "merge pops the reconvergence frame");

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "all lanes execute the merge ADD");
    check(warp.lanes[0].registers[3] == 100 &&
              warp.lanes[1].registers[3] == 100,
          "taken lanes keep the taken-path value");
    check(warp.lanes[2].registers[3] == 200 &&
              warp.lanes[3].registers[3] == 200,
          "fallthrough lanes keep the fallthrough-path value");
    check(warp.lanes[0].registers[4] == 100 &&
              warp.lanes[1].registers[4] == 101 &&
              warp.lanes[2].registers[4] == 202 &&
              warp.lanes[3].registers[4] == 203,
          "reconverged ADD executes for all four lanes");

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Halted,
          "branch demo terminates after reconvergence");
}

void test_warp_exit_during_divergence_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::branch_if(0, 3, 4),
        minisimt::Instruction::exit(),
        minisimt::Instruction::jump(4),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::exit(),
    };
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 1;
    warp.lanes[1].registers[0] = 1;

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "test setup creates divergent paths");
    const minisimt::Warp before_exit = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Error,
          "EXIT before reconvergence returns Error");
    check(warp == before_exit,
          "failed divergent EXIT changes no warp state");
}

void test_warp_unstructured_jump_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::branch_if(0, 4, 5),
        minisimt::Instruction::jump(3),
        minisimt::Instruction::mov_imm(1, 10),
        minisimt::Instruction::jump(5),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::exit(),
    };
    minisimt::Warp warp;
    warp.lanes[0].registers[0] = 1;
    warp.lanes[1].registers[0] = 1;

    check(minisimt::step(program, warp) ==
              minisimt::StepResult::Executed,
          "test setup creates divergence before an unstructured JUMP");
    const minisimt::Warp before_jump = warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Error,
          "JUMP away from the reconvergence point returns Error");
    check(warp == before_jump,
          "failed divergent JUMP changes no warp state");
}

void test_scalar_memory_program()
{
    const minisimt::Program program{
        minisimt::Instruction::lane_id(0),
        minisimt::Instruction::load(1, 0, 1),
        minisimt::Instruction::mov_imm(2, 77),
        minisimt::Instruction::store(0, 2, 2),
        minisimt::Instruction::exit(),
    };
    minisimt::ThreadContext context;
    minisimt::GlobalMemory memory{10, 20, 30};

    minisimt::StepResult result = minisimt::StepResult::Executed;
    while (result == minisimt::StepResult::Executed) {
        result = minisimt::step(program, context, memory);
    }

    check(result == minisimt::StepResult::Halted,
          "scalar memory program reaches EXIT");
    check(context.registers[0] == 0,
          "scalar LANE_ID writes lane zero");
    check(context.registers[1] == 20,
          "scalar LD reads base plus offset");
    check(memory[2] == 77,
          "scalar ST writes base plus offset");
    check(context.pc == 4,
          "scalar memory program leaves pc at EXIT");
}

void test_memory_instruction_requires_memory()
{
    const minisimt::Program program{
        minisimt::Instruction::load(1, 0),
    };
    minisimt::ThreadContext context;
    const minisimt::ThreadContext before = context;

    const auto result = minisimt::step(program, context);

    check(result == minisimt::StepResult::Error,
          "LD without a memory object returns Error");
    check(context == before,
          "LD without memory changes no thread state");
}

void test_warp_lane_id()
{
    const minisimt::Program program{
        minisimt::Instruction::lane_id(0),
    };
    minisimt::Warp warp;

    const auto result = minisimt::step(program, warp);

    check(result == minisimt::StepResult::Executed,
          "warp LANE_ID returns Executed");
    check(warp.pc == 1,
          "warp LANE_ID advances the shared pc once");
    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        check(
            warp.lanes[lane].registers[0] ==
                static_cast<int>(lane),
            "LANE_ID writes the physical lane index");
    }
}

void test_inactive_lane_does_not_access_memory()
{
    const minisimt::Program program{
        minisimt::Instruction::load(1, 0),
    };
    minisimt::Warp warp;
    minisimt::GlobalMemory memory{10, 20, 30, 40};
    warp.active_mask = minisimt::ActiveMask{0b0101};
    warp.lanes[0].registers[0] = 0;
    warp.lanes[1].registers[0] = 100;
    warp.lanes[2].registers[0] = 2;
    warp.lanes[3].registers[0] = -1;
    for (auto& lane : warp.lanes) {
        lane.registers[1] = -1;
    }

    const auto result = minisimt::step(program, warp, memory);

    check(result == minisimt::StepResult::Executed,
          "inactive invalid addresses do not reject LD");
    check(warp.lanes[0].registers[1] == 10,
          "active lane 0 loads memory");
    check(warp.lanes[1].registers[1] == -1,
          "inactive lane 1 performs no load");
    check(warp.lanes[2].registers[1] == 30,
          "active lane 2 loads memory");
    check(warp.lanes[3].registers[1] == -1,
          "inactive lane 3 performs no load");
}

void test_warp_memory_error_is_atomic()
{
    const minisimt::Program program{
        minisimt::Instruction::store(0, 1),
    };
    minisimt::Warp warp;
    minisimt::GlobalMemory memory{1, 2, 3, 4};

    for (std::size_t lane = 0; lane < warp.lanes.size(); ++lane) {
        warp.lanes[lane].registers[0] = static_cast<int>(lane);
        warp.lanes[lane].registers[1] =
            static_cast<int>(lane) + 10;
    }
    warp.lanes[3].registers[0] = 4;

    const minisimt::Warp before_warp = warp;
    const minisimt::GlobalMemory before_memory = memory;

    const auto result = minisimt::step(program, warp, memory);

    check(result == minisimt::StepResult::Error,
          "one active out-of-range ST returns Error");
    check(warp == before_warp,
          "failed warp ST changes no warp state");
    check(memory == before_memory,
          "failed warp ST performs no partial memory writes");
}

void test_vector_add_kernel()
{
    constexpr int b_base = 4;
    constexpr int c_base = 8;
    const minisimt::Program program{
        minisimt::Instruction::lane_id(0),
        minisimt::Instruction::load(1, 0),
        minisimt::Instruction::load(2, 0, b_base),
        minisimt::Instruction::add(3, 1, 2),
        minisimt::Instruction::store(0, 3, c_base),
        minisimt::Instruction::exit(),
    };
    minisimt::GlobalMemory memory{
        1, 2, 3, 4,
        10, 20, 30, 40,
        -1, -1, -1, -1,
    };
    minisimt::Warp warp;

    minisimt::StepResult result = minisimt::StepResult::Executed;
    while (result == minisimt::StepResult::Executed) {
        result = minisimt::step(program, warp, memory);
    }

    check(result == minisimt::StepResult::Halted,
          "vector-add kernel reaches EXIT");
    check(warp.pc == 5,
          "vector-add kernel leaves pc at EXIT");
    check(memory[8] == 11 &&
              memory[9] == 22 &&
              memory[10] == 33 &&
              memory[11] == 44,
          "four lanes compute the vector-add output");
}

void test_memory_trace_records_lane_accesses()
{
    const minisimt::Program program{
        minisimt::Instruction::load(1, 0),
    };
    minisimt::Warp warp;
    minisimt::GlobalMemory memory{10, 20, 30, 40};
    warp.active_mask = minisimt::ActiveMask{0b0101};
    warp.lanes[0].registers[0] = 0;
    warp.lanes[2].registers[0] = 2;
    minisimt::WarpTraceRecord record;

    const auto result =
        minisimt::step_with_trace(program, warp, memory, record);
    const std::string json = minisimt::format_trace_json(record);

    check(result == minisimt::StepResult::Executed,
          "traced LD returns Executed");
    check(record.memory_accesses.size() == 2,
          "trace records one access per active lane");
    check(record.memory_accesses[0] ==
              minisimt::MemoryAccessRecord{
                  0,
                  minisimt::MemoryAccessKind::Load,
                  0,
                  10,
              },
          "trace records lane 0 load");
    check(record.memory_accesses[1] ==
              minisimt::MemoryAccessRecord{
                  2,
                  minisimt::MemoryAccessKind::Load,
                  2,
                  30,
              },
          "trace records lane 2 load");
    check(
        json.find(
            "\"memory_accesses\":[{\"lane\":0,\"kind\":\"load\","
            "\"address\":0,\"value\":10}") != std::string::npos,
        "trace JSON serializes memory access metadata");
}

void test_branch_trace_records_reconvergence_frame()
{
    const minisimt::Program program = make_branch_program();
    minisimt::Warp warp;
    static_cast<void>(minisimt::step(program, warp));
    static_cast<void>(minisimt::step(program, warp));
    static_cast<void>(minisimt::step(program, warp));
    minisimt::WarpTraceRecord record;

    const auto result =
        minisimt::step_with_trace(program, warp, record);
    const std::string json = minisimt::format_trace_json(record);

    check(result == minisimt::StepResult::Executed,
          "traced divergent BRA_IF returns Executed");
    check(record.before.reconvergence_stack.empty(),
          "branch trace starts without a reconvergence frame");
    check(record.after.reconvergence_stack.size() == 1,
          "branch trace captures the pushed frame");
    check(record.after.active_mask ==
              minisimt::ActiveMask{0b1100},
          "branch trace captures the fallthrough mask");
    check(
        json.find(
            "\"opcode\":\"BRA_IF\",\"dst\":-1,\"src1\":2,"
            "\"src2\":-1,\"immediate\":0,\"target\":6,"
            "\"reconverge\":7") != std::string::npos,
        "branch trace serializes control-flow operands");
    check(
        json.find(
            "\"pending_mask\":\"0011\",\"pending_pc\":6,"
            "\"reconverge_pc\":7,"
            "\"pending_path_started\":false") !=
            std::string::npos,
        "branch trace serializes the reconvergence frame");
}

void test_warp_trace_record()
{
    const minisimt::Program program{
        minisimt::Instruction::mul(2, 0, 1),
    };
    minisimt::Warp warp;
    warp.active_mask = minisimt::ActiveMask{0b0101};
    warp.lanes[0].registers[0] = 3;
    warp.lanes[0].registers[1] = 4;
    warp.lanes[2].registers[0] = 5;
    warp.lanes[2].registers[1] = 6;
    const minisimt::Warp before = warp;
    minisimt::WarpTraceRecord record;

    const auto result =
        minisimt::step_with_trace(program, warp, record);

    check(result == minisimt::StepResult::Executed,
          "traced MUL returns Executed");
    check(record.before == before,
          "trace stores the complete before snapshot");
    check(record.instruction.has_value() &&
              record.instruction.value() == program[0],
          "trace stores the fetched instruction");
    check(record.result == result,
          "trace stores the step result");
    check(record.after == warp,
          "trace stores the complete after snapshot");
    check(record.after.lanes[0].registers[2] == 12,
          "trace captures lane 0 product");
    check(record.after.lanes[2].registers[2] == 30,
          "trace captures lane 2 product");
}

void test_warp_trace_without_fetch()
{
    const minisimt::Program program;
    minisimt::Warp warp;
    const minisimt::Warp before = warp;
    minisimt::WarpTraceRecord record;

    const auto result =
        minisimt::step_with_trace(program, warp, record);

    check(result == minisimt::StepResult::Error,
          "trace preserves invalid pc result");
    check(!record.instruction.has_value(),
          "trace uses null instruction when fetch does not occur");
    check(record.before == before && record.after == before,
          "failed fetch trace records unchanged state");
}

void test_warp_trace_json()
{
    const minisimt::Program program{
        minisimt::Instruction::exit(),
    };
    minisimt::Warp warp;
    warp.active_mask = minisimt::ActiveMask{0b0011};
    minisimt::WarpTraceRecord record;

    const auto result =
        minisimt::step_with_trace(program, warp, record);
    const std::string json = minisimt::format_trace_json(record);

    check(result == minisimt::StepResult::Halted,
          "traced EXIT returns Halted");
    check(!json.empty() && json.front() == '{' && json.back() == '}',
          "trace JSON is one complete object");
    check(json.find('\n') == std::string::npos,
          "trace JSON contains no embedded newline");
    check(json.find("\"opcode\":\"EXIT\"") != std::string::npos,
          "trace JSON includes the opcode");
    check(json.find("\"active_mask\":\"0011\"") != std::string::npos,
          "trace JSON includes the before mask");
    check(json.find("\"active_mask\":\"0000\"") != std::string::npos,
          "trace JSON includes the after mask");
    check(json.find("\"result\":\"Halted\"") != std::string::npos,
          "trace JSON includes the result");
}

} // namespace

int main()
{
    test_mov_immediate();
    test_negative_immediate();
    test_register_move();
    test_add();
    test_mul();
    test_cmp_lt();
    test_exit();
    test_invalid_register_is_atomic();
    test_malformed_instruction_is_atomic();
    test_invalid_pc_is_atomic();
    test_program_flow();
    test_scalar_branch_flow();
    test_invalid_branch_target_is_atomic();
    test_malformed_branch_layout_is_atomic();
    test_warp_executes_four_lanes();
    test_warp_active_mask();
    test_warp_mul();
    test_warp_invalid_instruction_is_atomic();
    test_warp_invalid_pc_is_atomic();
    test_warp_rejects_active_finished_lane_atomically();
    test_warp_empty_mask_is_halted();
    test_warp_exit();
    test_warp_program_flow();
    test_warp_uniform_branch_avoids_stack();
    test_warp_branch_divergence_and_reconvergence();
    test_warp_exit_during_divergence_is_atomic();
    test_warp_unstructured_jump_is_atomic();
    test_scalar_memory_program();
    test_memory_instruction_requires_memory();
    test_warp_lane_id();
    test_inactive_lane_does_not_access_memory();
    test_warp_memory_error_is_atomic();
    test_vector_add_kernel();
    test_memory_trace_records_lane_accesses();
    test_branch_trace_records_reconvergence_frame();
    test_warp_trace_record();
    test_warp_trace_without_fetch();
    test_warp_trace_json();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All MiniSIMT tests passed ("
              << assertions << " assertions)\n";
    return 0;
}
