#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace expression {
enum class VectorStatistic { Sum, Product, Mean, Minimum, Maximum, Median, Mode, Variance, Deviation };

inline std::optional<VectorStatistic> vectorStatistic(std::string_view name) {
    if (name == "sum") return VectorStatistic::Sum;
    if (name == "prod") return VectorStatistic::Product;
    if (name == "mean") return VectorStatistic::Mean;
    if (name == "min") return VectorStatistic::Minimum;
    if (name == "max") return VectorStatistic::Maximum;
    if (name == "median") return VectorStatistic::Median;
    if (name == "mode") return VectorStatistic::Mode;
    if (name == "var") return VectorStatistic::Variance;
    if (name == "std") return VectorStatistic::Deviation;
    return std::nullopt;
}

inline double evaluateStatistic(VectorStatistic operation, const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    if (std::any_of(values.begin(), values.end(), [](double value) { return std::isnan(value); }))
        return std::numeric_limits<double>::quiet_NaN();
    switch (operation) {
        case VectorStatistic::Sum:
            return std::accumulate(values.begin(), values.end(), 0.0);
        case VectorStatistic::Product:
            return std::accumulate(values.begin(), values.end(), 1.0, std::multiplies<double>{});
        case VectorStatistic::Minimum:
            return *std::min_element(values.begin(), values.end());
        case VectorStatistic::Maximum:
            return *std::max_element(values.begin(), values.end());
        case VectorStatistic::Median: {
            auto sorted = values;
            std::sort(sorted.begin(), sorted.end());
            const auto middle = sorted.size() / 2;
            return sorted.size() % 2 ? sorted[middle] : std::midpoint(sorted[middle - 1], sorted[middle]);
        }
        case VectorStatistic::Mode: {
            std::map<double, size_t> counts;
            for (double value : values) ++counts[value];
            size_t most = 0;
            double result = values.front();
            for (const auto& [value, count] : counts) {
                if (count > most) { most = count; result = value; }
            }
            return result;
        }
        default: {
            long double mean = 0.0L;
            for (double value : values) mean += static_cast<long double>(value) / values.size();
            if (operation == VectorStatistic::Mean) return static_cast<double>(mean);
            if (values.size() == 1) return 0.0;
            long double deviation = 0.0L;
            const long double scale = std::sqrt(static_cast<long double>(values.size() - 1));
            for (double value : values) {
                const long double centered = static_cast<long double>(value) - mean;
                const long double difference = std::isfinite(centered) ? centered / scale : static_cast<long double>(value) / scale - mean / scale;
                deviation = std::hypot(deviation, difference);
            }
            return static_cast<double>(operation == VectorStatistic::Deviation ? deviation : deviation * deviation);
        }
    }
}

inline double vectorDot(const std::vector<double>& left, const std::vector<double>& right, std::string& error) {
    if (left.size() != right.size()) {
        error = "Array size mismatch (" + std::to_string(left.size()) + " vs " + std::to_string(right.size()) + ")";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::inner_product(left.begin(), left.end(), right.begin(), 0.0);
}

inline std::vector<double> vectorElementwise(std::string_view operation,
    const std::vector<double>* left, double leftScalar,
    const std::vector<double>* right, double rightScalar, std::string& error) {
    if (left && right && left->size() != right->size()) {
        error = "Array size mismatch (" + std::to_string(left->size()) + " vs " + std::to_string(right->size()) + ")";
        return {};
    }
    const size_t count = left ? left->size() : right ? right->size() : 0;
    std::vector<double> result(count);
    for (size_t i = 0; i < count; ++i) {
        const double a = left ? (*left)[i] : leftScalar;
        const double b = right ? (*right)[i] : rightScalar;
        if (operation == "+") result[i] = a + b;
        else if (operation == "-") result[i] = a - b;
        else if (operation == ".*" || (operation == "*" && !(left && right))) result[i] = a * b;
        else if (operation == "./" || (operation == "/" && !(left && right))) {
            if (b == 0.0) { error = "Division by zero"; return {}; }
            result[i] = a / b;
        } else {
            error = "Invalid operator: " + std::string(operation);
            return {};
        }
    }
    return result;
}

inline std::vector<double> vectorRange(double start, double step, double end, size_t limit, std::string& error) {
    if (!std::isfinite(start) || !std::isfinite(step) || !std::isfinite(end) || step == 0.0) {
        error = "Invalid range bounds or step";
        return {};
    }
    const long double span = (static_cast<long double>(end) - start) / step;
    if (span < 0) return {};
    if (!std::isfinite(span) || span >= limit) {
        error = "Overflow Error: array range limit exceeded";
        return {};
    }
    const size_t count = static_cast<size_t>(std::floor(span + 1e-12L)) + 1;
    std::vector<double> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) result.push_back(start + step * static_cast<double>(i));
    return result;
}
}
