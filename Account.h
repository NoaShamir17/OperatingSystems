#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "LockRW.h"

// Mode indicators for locking

class Account {
private:

    // Reader-Writer lock specifically for this account 
    LockRW accountLock; 

public:
    // Account details
    int id;
    int password;
    int balanceILS;
    int balanceUSD;

    // Constructor & Destructor
    Account(int id, int password, int initILS, int initUSD);
    Account(const Account& other);
    ~Account();
    
    Account& operator=(const Account& other);

    // Getters require read locks, Setters/Actions require write locks
    bool checkPassword(int pwd);
    
    // // Core Actions (Thread Safe internally)
    // void deposit(int amount, bool isILS); 
    // bool withdraw(int amount, bool isILS); 
    // void getBalance(int &ils, int &usd);  
    
    // For Bank Commission
    // Deducts commission from the account balance and prints to log
    void takeCommission(double percentage); 
    
    // Helper to lock the account explicitly (e.g. for Transfer)
    // Be careful with deadlocks here!
    void lockAccount(bool writeMode);
    void unlockAccount(bool writeMode);
};

#endif