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
            return sorted.size() % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2.0;
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
            const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            if (operation == VectorStatistic::Mean) return mean;
            if (values.size() == 1) return 0.0;
            double squares = 0.0;
            for (double value : values) squares += (value - mean) * (value - mean);
            const double variance = squares / (values.size() - 1);
            return operation == VectorStatistic::Deviation ? std::sqrt(variance) : variance;
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
}
