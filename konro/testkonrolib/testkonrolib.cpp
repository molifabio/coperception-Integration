#include "konrolib.h"
#include <iostream>
#include <thread>
#include <string>

using namespace std;

int main(int argc, char *argv[])
{
    string reply = konro::sendAddMessage();
    if (reply.empty()) {
        cout << "Received empty reply (invalid server address?)" << endl;
    } else {
        cout << "Received reply: " << reply << endl;
    }

    bool val = true;
    for (int i = 0; i < 10; ++i) {
        cout << "Sending message" << endl;
        reply = konro::sendFeedbackMessage(static_cast<int>(val));
        if (reply.empty()) {
            cout << "Received empty reply (invalid server address?)" << endl;
        } else {
            cout << "Received reply: " << reply << endl;
        }
        cout << "Sleeping" << endl;
        this_thread::sleep_for(chrono::seconds(6));
        val= !val;
   }
}
