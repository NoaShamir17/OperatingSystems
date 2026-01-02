#ifndef BANK_H
#define BANK_H

#include <map>
#include <list>
#include <vector>
#include <pthread.h>
#include <algorithm>
#include "LockRW.h"
#include "Account.h"
#include "ATM.h"

#define DOLLAR_TO_ILS_RATE 5 // 1 USD = 5 ILS

// Helper struct to store snapshot data (lightweight, no mutexes)
struct AccountSnapshot {
    int id;
    int password;
    int balanceILS;
    int balanceUSD;
};

class Bank {
public:
    std::map<int, Account*> accounts; // Map ID -> Account*
    std::map<int, ATM*> atms;         // Map ID -> ATM* 
    LockRW bankLock; // Protects the map structure (Open/Close account)
private:

    bool isWorking; // Flag for threads
    pthread_t commissionThread;
    pthread_t statusThread; //prints status periodically, updates status history, handles ATM close requests

    // --- History Management ---
    // List of maps. Each map is a "status" of the bank at a point in time.
    std::list<std::map<int, AccountSnapshot> > history; 
    pthread_mutex_t historyMutex;

    // Internal helper to create a snapshot
    void takeSnapshot();

    // Singleton instance
    Bank(); 

public:
    static Bank& getInstance();
    ~Bank();

    //----------------lock Management--------------------------
    void lockBank(bool writeMode);
    void unlockBank(bool writeMode);

    //----------------Account Management--------------------------

    // Returns true if successful, false if account ID already exists
    // BankLock Write Lock DOES NOT need to be held internally
    bool openAccount(int id, int pass, int initILS, int initUSD); 

    // Returns true if successful, false if account doesn't exist or wrong password
    // BankLock Write Lock DOES NOT need to be held internally
    bool closeAccount(int id, int pass); 
    Account* getAccount(int id); // Helper to find account

    //----------------ATM Functions--------------------------

    // Request to close an ATM (called by ATM)
    // The actual closing is handled by the Bank's status thread
    // uses ATM::close()
    void requestCloseATM(int requesterId, int targetId);

    // Rollback Command
    // Executed by ATM, locks the whole bank, restores state
    void rollback(int atmId, int steps);

    //----------------Thread Routines--------------------------

    // Bank Commission Loop (Thread function)
    static void* commissionRoutine(void* arg); // [cite: 74]
    
    // Status Printer Loop (Thread function)
    static void* statusRoutine(void* arg);

    //-----------------System control--------------------------
    void run();
    void stop();
};




#endif
