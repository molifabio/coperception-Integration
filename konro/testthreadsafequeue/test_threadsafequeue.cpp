#include "unittest.h"
#include "threadsafequeue.h"
#include <iostream>
#include <thread>

using namespace std;

static int testThreadsafeQueue1()
{
    rmcommon::ThreadsafeQueue<int> tsQueue;
    tsQueue.push(1);
    tsQueue.push(2);
    tsQueue.push(3);

    int val;
    if (!tsQueue.tryPop(val) || val != 1)
        return TEST_FAILED;
    if (!tsQueue.tryPop(val) || val != 2)
        return TEST_FAILED;
    if (!tsQueue.tryPop(val) || val != 3)
        return TEST_FAILED;
    if (!tsQueue.empty())
        return TEST_FAILED;
    return TEST_OK;
}

static int testThreadsafeQueue2()
{
    rmcommon::ThreadsafeQueue<int> tsQueue;
    int val;
    if (tsQueue.waitAndPop(val, std::chrono::milliseconds(100)))
        return TEST_FAILED;
    return TEST_OK;
}

class Consumer {
    rmcommon::ThreadsafeQueue<int> &queue_;
    long tot_;
public:
    Consumer(rmcommon::ThreadsafeQueue<int> &queue) : queue_(queue), tot_() { }

    void operator()() {
        int n;
        while (queue_.waitAndPop(n, chrono::milliseconds(50))) {
            tot_ += n;
        }
    }

    long tot() const { return tot_; }
};

/*!
 * Tests multiple consumers in separate threads
 *
 * \return Outcome of the test
 */
static int testThreadsafeQueue3()
{
    rmcommon::ThreadsafeQueue<int> tsQueue;
    Consumer c1(tsQueue), c2(tsQueue), c3(tsQueue);
    thread t1(ref(c1));
    thread t2(ref(c2));
    thread t3(ref(c3));

    long expectedTotal = 0;
    for (int i = 0; i < 100000; ++i) {
        tsQueue.push(i);
        expectedTotal += i;
    }
    t1.join();
    t2.join();
    t3.join();
    long threadTotal = c1.tot() + c2.tot() + c3.tot();
    if (threadTotal == expectedTotal) {
        cout << "Got " << c1.tot() << ", " << c2.tot() << ", " << c3.tot() << endl;
        cout << "(threadTotal) " << threadTotal << " == " << expectedTotal << " (tot)" << endl;
        return TEST_OK;
    } else {
        cout << "Expected " << expectedTotal << " got " << threadTotal << endl;
        return TEST_FAILED;
    }
}


int main(int argc, char *argv[])
{
    if (testThreadsafeQueue1() != TEST_OK) return TEST_FAILED;
    if (testThreadsafeQueue2() != TEST_OK) return TEST_FAILED;
    if (testThreadsafeQueue3() != TEST_OK) return TEST_FAILED;

   return TEST_OK;
}
