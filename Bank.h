#ifndef BANK_H
#define BANK_H

#include <map>
#include "Account.h"
#include "LockRW.h"

class Bank {
public:
    std::map<int, Account*> accounts; // Map ID -> Account*
    std::map<int, ATM*> atms;         // Map ID -> ATM* 
    LockRW bankLock; // Protects the map structure (Open/Close account)
private:
    int bankVaultILS;
    int bankVaultUSD;
    LockRW vaultLock; // Protects the bank's own profits

    bool isWorking; // Flag for threads
    pthread_t commissionThread;
    pthread_t printerThread;

    // Singleton instance
    Bank(); 

public:
    static Bank& getInstance();
    ~Bank();

    // Account Management

    // Returns true if successful, false if account ID already exists
    bool openAccount(int id, int pass, int initILS, int initUSD); // [cite: 58]

    // Returns true if successful, false if account doesn't exist or wrong password
    bool closeAccount(int id, int pass); // [cite: 59]
    Account* getAccount(int id); // Helper to find account

    // Bank Commission Loop (Thread function)
    static void* commissionRoutine(void* arg); // [cite: 74]
    
    // Status Printer Loop (Thread function)
    static void* printerRoutine(void* arg); // [cite: 240]

    // System control
    void run();
    void stop();
};

#endif
