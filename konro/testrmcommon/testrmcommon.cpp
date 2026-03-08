#include <iostream>
#include "cpusetvector.h"

#define TEST_OK     0
#define TEST_FAILED 1

using namespace std;
using namespace rmcommon;

static int test_toSet1()
{
    CpusetVector vec1{ {1, 2}, {4, 4}, {6, 9} };

    set<short> set1 = toSet(vec1);

    for (std::pair<short,short> p: vec1) {
        for (int n = p.first; n <= p.second; ++n) {
            if (set1.find(n) == set1.end()) {
                return TEST_FAILED;
            }
        }
    }
    if (set1.size() != countPUs(vec1)) {
        cout << "set1.size()    = " << set1.size()    << endl;
        cout << "countPUs(vec1) = " << countPUs(vec1) << endl;

        return TEST_FAILED;
    }
    return TEST_OK;
}

static int test_containsPU()
{
    CpusetVector vec1{ {1, 2}, {4, 4}, {6, 9} };

    if (!containsPU(vec1,  1)) return TEST_FAILED;
    if (!containsPU(vec1,  2)) return TEST_FAILED;
    if ( containsPU(vec1,  3)) return TEST_FAILED;
    if (!containsPU(vec1,  4)) return TEST_FAILED;
    if ( containsPU(vec1,  5)) return TEST_FAILED;
    if (!containsPU(vec1,  6)) return TEST_FAILED;
    if (!containsPU(vec1,  7)) return TEST_FAILED;
    if (!containsPU(vec1,  8)) return TEST_FAILED;
    if (!containsPU(vec1,  9)) return TEST_FAILED;
    if ( containsPU(vec1, 10)) return TEST_FAILED;
    return TEST_OK;
}

static int test_toCpusetVector_1()
{
    set<short> set1{1, 2, 4, 6, 7, 8, 9, 11, 13, 14, 15, 16, 17, 18, 19, 20, 22};
    CpusetVector vec1{ {1, 2}, {4, 4}, {6, 9}, {11, 11}, {13, 20}, {22, 22} };
    CpusetVector vec2 = toCpusetVector(set1);
    if (vec2 != vec1) return TEST_FAILED;
    return TEST_OK;
}

static int test_toCpusetVector_2()
{
    set<short> set1;
    CpusetVector vec1;
    CpusetVector vec2 = toCpusetVector(set1);
    if (vec2 != vec1) return TEST_FAILED;
    return TEST_OK;
}

static int test_toCpusetVector_3()
{
    set<short> set1{6};
    CpusetVector vec1{{6,6}};
    CpusetVector vec2 = toCpusetVector(set1);
    if (vec2 != vec1) return TEST_FAILED;
    return TEST_OK;
}

static int test_toString()
{
    CpusetVector vec1{ {1, 2}, {4, 4}, {6, 9}, {11, 11}, {13, 20}, {22, 22} };
    string s = "1-2,4,6-9,11,13-20,22";
    if (toString(vec1) != s) return TEST_FAILED;
    return TEST_OK;
}

int main(int argc, char *argv[])
{
    if (test_toSet1() != TEST_OK) return TEST_FAILED;
    if (test_containsPU() != TEST_OK) return TEST_FAILED;
    if (test_toCpusetVector_1() != TEST_OK) return TEST_FAILED;
    if (test_toCpusetVector_2() != TEST_OK) return TEST_FAILED;
    if (test_toCpusetVector_3() != TEST_OK) return TEST_FAILED;
    if (test_toString() != TEST_OK) return TEST_FAILED;

   return TEST_OK;

}
