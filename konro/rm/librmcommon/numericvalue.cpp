#include "numericvalue.h"
#include <cstring>
#include <cstdlib>

namespace rmcommon {

void NumericValue::init(const char *pStart, const char *pEnd)
{
    value_ = NUMERIC_VALUE_INVALID;
    if (!pStart)
        return;
    if (pEnd == nullptr)
        pEnd = pStart + strlen(pStart);     // assume zero terminated string
    if (pEnd - pStart == 3 && strncmp(pStart, "max", 3) == 0) {
        value_ = NUMERIC_VALUE_MAX;
        return;
    }
    char *endptr;
    value_ = strtoull(pStart, &endptr, 10);
    if (endptr != pEnd) {
        value_ = NUMERIC_VALUE_INVALID;
    }
}

NumericValue::NumericValue(const char *pStart, const char *pEnd)
{
    init(pStart, pEnd);
}

NumericValue::NumericValue(const std::string &val)
{
    const char *pStart = val.c_str();
    const char *pEnd = pStart + val.size();
    init(pStart, pEnd);
}

}   // namespace rmcommon
