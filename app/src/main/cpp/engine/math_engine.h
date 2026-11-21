#pragma once

#include "app_state.h"
#include "math_funcs.h"
#include "help_engine.h"

bool preprocessHelp(const std::string& s, std::string& help_out);
std::vector<UserFunc> getParsedUserFuncs();
double evaluate(const std::string& expr_str, std::string& err);
double evaluateScalar(const std::string& source, std::string& error, const char* context);
double evaluateWithoutHistory(const std::string& expr_str, std::string& err);
double evaluateInput(const std::string& input, std::string& err, bool& isDefinition);
size_t findAssignmentOperator(const std::string& input);
std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
std::string formatPrintString(const std::string& str, std::string& err);
double assignArrayElement(const std::string& name, const std::string& index,
                          const std::string& value, std::string& error);
bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr);
void handleTabCompletion(std::string& expression, int& cursor_pos);
void resetTabState();
std::vector<std::string> getConvUnitMatches(const std::string& from_unit, const std::string& prefix);

#include "plot_engine.h"
#include "script_engine.h"
