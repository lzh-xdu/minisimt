#include "minisimt/model.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
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

} // namespace

int main()
{
    test_mov_immediate();
    test_negative_immediate();
    test_register_move();
    test_add();
    test_exit();
    test_invalid_register_is_atomic();
    test_malformed_instruction_is_atomic();
    test_invalid_pc_is_atomic();
    test_program_flow();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All MiniSIMT tests passed\n";
    return 0;
}
