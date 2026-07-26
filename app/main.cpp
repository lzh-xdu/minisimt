#include "minisimt/model.hpp"

#include <iostream>

int main()
{
    const minisimt::Program program{
        minisimt::Instruction::mov_imm(0, 10),
        minisimt::Instruction::mov_imm(1, 20),
        minisimt::Instruction::add(2, 0, 1),
        minisimt::Instruction::mov_reg(3, 2),
        minisimt::Instruction::exit(),
    };

    minisimt::ThreadContext context;

    while (true) {
        if (context.pc < program.size()) {
            std::cout << "instruction="
                      << minisimt::format_instruction(program[context.pc])
                      << '\n';
        }

        std::cout << "before " << minisimt::format_context(context) << '\n';

        const minisimt::StepResult result =
            minisimt::step(program, context);

        std::cout << "after  result="
                  << minisimt::step_result_name(result)
                  << ' ' << minisimt::format_context(context)
                  << "\n\n";

        if (result != minisimt::StepResult::Executed) {
            return result == minisimt::StepResult::Halted ? 0 : 1;
        }
    }
}
