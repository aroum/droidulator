#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include "units_bridge.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const int initialized = bridge_init(UNITS_DAT_PATH);
    if (initialized != 0) std::abort();
    if (size > 512) return 0;
    const size_t split = size / 2;
    const std::string from(reinterpret_cast<const char *>(data), split);
    const std::string to(reinterpret_cast<const char *>(data + split), size - split);
    double value = 0;
    char unit[256] = {}, error[256] = {};
    bridge_convert(1.0, from.c_str(), to.c_str(), &value, unit, sizeof(unit), error, sizeof(error));
    return 0;
}
