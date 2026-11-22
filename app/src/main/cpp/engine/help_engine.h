#ifndef HELP_ENGINE_H
#define HELP_ENGINE_H

#include <string>
#include <vector>
#include "app_state.h"

// Parse help requests like help(), help(plot), help(sin), etc.
bool preprocessHelp(const std::string& input_expr, std::string& help_out);

#endif // HELP_ENGINE_H
