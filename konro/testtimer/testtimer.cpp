#include "timer.h"
#include <iostream>
#include <ctime>
#include <unistd.h>

using namespace std;
using namespace rmcommon;

int main(int argc, char *argv[])
{
    struct timespec ts1, ts2;

    memset(&ts1, 0, sizeof(ts1));
    memset(&ts2, 0, sizeof(ts2));

    KonroTimer timer;

    long micros1 = KonroTimer::getSystemMicroseconds();
    timer.Restart();
    sleep(3);
    long micros2 = KonroTimer::getSystemMicroseconds();
    KonroTimer::TimeUnit ms = timer.Elapsed();

    cout << "Timespec difference = " << (micros2 - micros1) << " microseconds" << endl;
    cout << "Timer difference    = " << ms.count() << " microseconds" << endl;
}
