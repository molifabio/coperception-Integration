#include "unittest.h"
#include "cgroup/cgrouputil.h"
#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <iomanip>

using namespace std;

static int testFind1()
{
    string path1 = pc::util::findCgroupPath((pid_t)1);
    if (path1 != "/sys/fs/cgroup/init.scope")
        return TEST_FAILED;
    return TEST_OK;
}

int main(int argc, char *argv[])
{
    if (testFind1() != TEST_OK)
        return TEST_FAILED;

   return TEST_OK;
}
