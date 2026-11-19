#pragma once

#include <string>
#include <vector>
#include "app_state.h"
#include "expression_tokens.h"

inline std::string cleanQuotes(const std::string& str) {
    if (str.size() >= 2 && (str.front() == '"' || str.front() == '\'') && str.back() == str.front()) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

bool handlePlotCommands(const std::string& line, std::string& err, double& result);
