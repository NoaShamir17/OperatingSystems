#include "Bank.h"
#include "ATM.h"

#include <pthread.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>

// ------------------------------------------------------------
// Program entry point
// Usage: ./bank <num_vip_threads> <atm_file_1> <atm_file_2> ...
//
// Responsibilities:
// 1) Create the Bank background threads (status/commissions/VIP consumers)
// 2) Create one ATM thread per input file
// 3) Wait for all ATMs to finish, then stop the Bank and exit
// ------------------------------------------------------------

static bool parseNonNegativeInt(const char* s, int& out) {
    if (s == NULL || *s == '\0') return false;
    char* end = NULL;
    long v = std::strtol(s, &end, 10);
    if (end == NULL || *end != '\0') return false;
    if (v < 0 || v > 1000000) return false;
    out = static_cast<int>(v);
    return true;
}

int main(int argc, char* argv[]) {
    // Minimal argument validation per assignment style
    if (argc < 3) {
        std::cerr << "Bank error: illegal arguments" << std::endl;
        return 1;
    }

    int vipThreads = 0;
    if (!parseNonNegativeInt(argv[1], vipThreads)) {
        std::cerr << "Bank error: illegal arguments" << std::endl;
        return 1;
    }

    // Create ATM objects (IDs are 1..N in the order of input files)
    std::vector<ATM*> atms;
    atms.reserve(static_cast<size_t>(argc - 2));

    Bank& bank = Bank::getInstance();

    // Register ATMs in the bank (used for the 'C' command)
    // We do this before starting any threads.
    bank.lockBank(WRITER_MODE);
    for (int i = 2; i < argc; ++i) {
        const int atmId = i - 1;
        ATM* atm = new ATM(atmId, std::string(argv[i]));
        atms.push_back(atm);
        bank.atms[atmId] = atm;
    }
    bank.unlockBank(WRITER_MODE);

    // Start bank background threads (status/commissions/VIP consumers)
    bank.run(vipThreads);

    // Start ATM threads
    std::vector<pthread_t> atmThreads;
    atmThreads.reserve(atms.size());

    for (size_t i = 0; i < atms.size(); ++i) {
        pthread_t t;
        if (pthread_create(&t, NULL, ATM::startRoutine, atms[i]) != 0) {
            std::cerr << "Bank error: illegal arguments" << std::endl;

            // Best-effort shutdown if thread creation fails
            for (size_t j = 0; j < i; ++j) {
                atms[j]->close();
            }
            for (size_t j = 0; j < atmThreads.size(); ++j) {
                pthread_join(atmThreads[j], NULL);
            }
            bank.stop();
            for (size_t j = 0; j < atms.size(); ++j) {
                delete atms[j];
            }
            return 1;
        }
        atmThreads.push_back(t);
    }

    // Wait for all ATMs to finish
    for (size_t i = 0; i < atmThreads.size(); ++i) {
        pthread_join(atmThreads[i], NULL);
    }

    // Stop bank background threads
    bank.stop();

    // Cleanup ATM objects
    bank.lockBank(WRITER_MODE);
    for (size_t i = 0; i < atms.size(); ++i) {
        bank.atms.erase(atms[i]->getId());
        delete atms[i];
    }
    bank.unlockBank(WRITER_MODE);

    return 0;
}
