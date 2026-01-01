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
    pthread_t statusThread; //prints status periodically, updates status history, handles ATM close requests

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

    //----------------ATM Management--------------------------

    // Request to close an ATM (called by ATM)
    // The actual closing is handled by the Bank's status thread
    // uses ATM::close()
    void requestCloseATM(int requesterId, int targetId);

    //----------------Thread Routines--------------------------

    // Bank Commission Loop (Thread function)
    static void* commissionRoutine(void* arg); // [cite: 74]
    
    // Status Printer Loop (Thread function)
    static void* statusRoutine(void* arg);

    // System control
    void run();
    void stop();
};

#endif
