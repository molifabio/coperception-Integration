#include "unittest.h"
#include "tsplit.h"

#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <iomanip>

using namespace std;

static int testSplit1()
{
    vector<string> parts = rmcommon::tsplit(string("/1/2/3"), "/");
    if (parts.size() != 4)
        return TEST_FAILED;
    if (parts[0] != "")
        return TEST_FAILED;
    if (parts[1] != "1")
        return TEST_FAILED;
    if (parts[2] != "2")
        return TEST_FAILED;
    if (parts[3] != "3")
        return TEST_FAILED;
    return TEST_OK;
}

static int testSplit2()
{
    vector<string> parts = rmcommon::tsplit(string("1/2/3"), "/");
    if (parts.size() != 3)
        return TEST_FAILED;
    if (parts[0] != "1")
        return TEST_FAILED;
    if (parts[1] != "2")
        return TEST_FAILED;
    if (parts[2] != "3")
        return TEST_FAILED;
    return TEST_OK;
}

static int testSplit3()
{
    vector<string> parts = rmcommon::tsplit(string("0::/init.scope"), ":");
    if (parts.size() != 3)
        return TEST_FAILED;
    if (parts[0] != "0")
        return TEST_FAILED;
    if (!parts[1].empty())
        return TEST_FAILED;
    if (parts[2] != "/init.scope")
        return TEST_FAILED;
    return TEST_OK;
}

int main(int argc, char *argv[])
{
    if (testSplit1() != TEST_OK) return TEST_FAILED;
    if (testSplit2() != TEST_OK) return TEST_FAILED;
    if (testSplit3() != TEST_OK) return TEST_FAILED;

   return TEST_OK;
}
