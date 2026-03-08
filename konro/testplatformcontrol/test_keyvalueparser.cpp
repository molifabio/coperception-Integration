#include "unittest.h"
#include "numericvalue.h"
#include "keyvalueparser.h"
#include "pcexception.h"

#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <iomanip>
#include <map>

using namespace std;
using namespace pc;

static int testParseNvSyntax(int major, int minor, const char *line, bool expectedError = false)
{
    try {
        map<string, rmcommon::NumericValue> tags = pc::KeyValueParser().parseLineNv(line, major, minor);
        if (expectedError) {
            return TEST_FAILED;
        }
#if 1
        if (tags.empty()) {
            cout << "    No tags found\n";
        } else {
            for (const auto &kp : tags) {
                cout << "    TAG: " << kp.first << ", VALUE: " << kp.second << endl;
            }
        }
#endif
    } catch (runtime_error &e) {
        if (!expectedError) {
            return TEST_FAILED;
        }
    }
    return TEST_OK;
}

static int testParseNvValues1()
{
    const char line[] = "8:0 abcd=12345 efgh=max pippo=7 pluto=12345678";

    map<string, rmcommon::NumericValue> tags = pc::KeyValueParser().parseLineNv(line, 8, 0);
    if (tags.empty())
        return TEST_FAILED;
    if (tags["abcd"] != 12345)
        return TEST_FAILED;
    if (!tags["efgh"].isMax())
        return TEST_FAILED;
    if (tags["pippo"] != 7)
        return TEST_FAILED;
    if (tags["pluto"] != 12345678)
        return TEST_FAILED;

    return TEST_OK;
}

static int testParseNvValues2()
{
    const char line[] = "8:0";

    map<string, rmcommon::NumericValue> tags = pc::KeyValueParser().parseLineNv(line, 8, 0);
    if (!tags.empty())
        return TEST_FAILED;

    return TEST_OK;
}

static int testParseKeyValue()
{
    pair<string, rmcommon::NumericValue> result = pc::KeyValueParser().parseKeyValue("alfa=33");
    if (result.first != "alfa" || result.second != 33)
        return TEST_FAILED;

    result = pc::KeyValueParser().parseKeyValue("alfa:33");
    if (result.first != "alfa" || result.second != 33)
        return TEST_FAILED;

    result = pc::KeyValueParser().parseKeyValue("alfa:max");
    if (result.first != "alfa" || !result.second.isMax())
        return TEST_FAILED;

    result = pc::KeyValueParser().parseKeyValue("alfa=max");
    if (result.first != "alfa" || !result.second.isMax())
        return TEST_FAILED;

    try {
        result = pc::KeyValueParser().parseKeyValue("alfa:abcd");
        return TEST_FAILED;
    } catch (runtime_error &e) {
        ;
    }

    try {
        result = pc::KeyValueParser().parseKeyValue("alfa:123a");
        return TEST_FAILED;
    } catch (runtime_error &e) {
        ;
    }

    try {
        result = pc::KeyValueParser().parseKeyValue("alfa: 123");
        return TEST_FAILED;
    } catch (runtime_error &e) {
        ;
    }

    try {
        result = pc::KeyValueParser().parseKeyValue("alfa123");
        return TEST_FAILED;
    } catch (runtime_error &e) {
        ;
    }

    return TEST_OK;
}

int main(int argc, char *argv[])
{
    // expected to be successful

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=54321 pippo=7 pluto=12345678") != TEST_OK)
        return TEST_FAILED;
    if (testParseNvSyntax(8, 1, "8:0 abcd=12345 efgh=54321 pippo=7 pluto=12345678") != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=max pippo=7 pluto=12345678") != TEST_OK)
        return TEST_FAILED;

    // expected to fail

    if (testParseNvSyntax(8, 0, "8.0 abcd=12345 efgh=max pippo=7 pluto=12345678", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0:abcd=12345 efgh=max pippo=7 pluto=12345678", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0  abcd=12345 efgh=max pippo=7 pluto=12345678", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "x:0 abcd=12345 efgh=max pippo=7 pluto=12345678", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 =12345 efgh=max pippo=7 pluto", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=max pippo=7 pluto", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=max pippo=7 pluto=", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:a abcd=12345 efgh=max pippo=7 pluto=12345678", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=max pippo=7 pluto=1234567a", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd =12345 efgh=max pippo=7 pluto=1234567", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd= 12345 efgh=max pippo=7 pluto=1234567", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=1 2345 efgh=max pippo=7 pluto=1234567", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345  efgh=max pippo=7 pluto=1234567a", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvSyntax(8, 0, "8:0 abcd=12345 efgh=max pippo=7 pluto=1234567 ", true) != TEST_OK)
        return TEST_FAILED;

    if (testParseNvValues1() != TEST_OK)
        return TEST_FAILED;

    if (testParseNvValues2() != TEST_OK)
        return TEST_FAILED;

    if (testParseKeyValue() != TEST_OK)
        return TEST_FAILED;

    return TEST_OK;
}
