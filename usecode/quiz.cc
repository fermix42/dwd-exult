#include "quiz.h"
#include "ucmachine.h"
#include <quiz_compat.h>

#include <sstream>
#include <string>

void load_builtin_quiz(Usecode_machine& machine, bool black_gate) {
    const auto* bytes = black_gate ? quiz_bg : quiz_si;
    const auto size = black_gate ? sizeof(quiz_bg) : sizeof(quiz_si);
    std::istringstream resource(std::string(reinterpret_cast<const char*>(bytes), size), std::ios::binary);
    machine.read_usecode(resource, true);
}
