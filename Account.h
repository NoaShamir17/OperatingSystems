#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "LockRW.h"

class Account {
private:
    int id;
    int password;
    int balanceILS;
    int balanceUSD;
    
    // Reader-Writer lock specifically for this account 
    LockRW accountLock; 

public:
    Account(int id, int password, int initILS, int initUSD);
    ~Account();

    // Getters require read locks, Setters/Actions require write locks
    int getId() const { return id; }
    bool checkPassword(int pwd);
    
    // Core Actions (Thread Safe internally)
    void deposit(int amount, bool isILS); // [cite: 60]
    bool withdraw(int amount, bool isILS); // [cite: 61]
    void getBalance(int &ils, int &usd);   // [cite: 62]
    
    // For Bank Commission (VIP/System use)
    void takeCommission(double percentage); // [cite: 75]
    
    // Helper to lock the account explicitly (e.g. for Transfer)
    // Be careful with deadlocks here!
    void lockAccount(bool writeMode);
    void unlockAccount(bool writeMode);
};

#endif