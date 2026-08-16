#pragma once

#include <algorithm>

#if !defined(__cpp_lib_ranges)
namespace std {
namespace ranges {
    template<typename Range, typename OutputIt>
    inline OutputIt copy(const Range& r, OutputIt out) {
        return std::copy(r.begin(), r.end(), out);
    }
}
}
#endif
