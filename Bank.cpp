#include "Bank.h"
#include "LogFile.h"
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cstdint>

//=====================Noa Added These Methods=========================
//lock Management
void Bank::lockBank(bool writeMode) {
    if (writeMode == WRITER_MODE) {
        bankLock.writeEnter();
    } else {
        bankLock.readEnter();
    }
}

void Bank::unlockBank(bool writeMode) {
    if (writeMode == WRITER_MODE) {
        bankLock.writeExit();
    } else {
        bankLock.readExit();
    }
}


// --------------------------------------------------------------------------
// Take Snapshot (Atomic View)
// --------------------------------------------------------------------------
void Bank::takeSnapshot() {
    // 1. Lock Bank for Reading
    // This ensures no accounts are added/deleted while we iterate.
    lockBank(READER_MODE); 

    // 2. Lock ALL Accounts (Ascending Order)
    // We must freeze the state of every account *before* we print or save anything
    // to ensure the snapshot is atomic (consistent point in time).
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        it->second->lockAccount(READER_MODE);
    }

    // --- CRITICAL SECTION: ALL ACCOUNTS ARE FROZEN ---

    std::map<int, AccountSnapshot> currentSnapshot;
    
    // 3. Clear Screen and Print Header
    //
    printf("\033[2J");   
    printf("\033[1;1H"); 
    printf("Current Bank Status\n");

    // 4. Iterate, Print, and Save Data
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        Account* acc = it->second;
        
        // Save to struct for History
        AccountSnapshot snap;
        snap.id = acc->id;
        snap.password = acc->password;
        snap.balanceILS = acc->balanceILS;
        snap.balanceUSD = acc->balanceUSD;
        
        currentSnapshot[acc->id] = snap;

        // Print strict format
        // "Account <id>: Balance - <ILS> ILS <USD> USD, Account Password - <pass>"
        printf("Account %d: Balance - %d ILS %d USD, Account Password - %d\n", 
               acc->id, acc->balanceILS, acc->balanceUSD, acc->password);
    }

    // Make sure the periodic status printer is visible immediately (important under output redirection)
    fflush(stdout);

    // 5. Update History (Thread Safe push)
    pthread_mutex_lock(&historyMutex);
    
    history.push_back(currentSnapshot);
    
    // Maintain max 100 snapshots
    // "remember 100-120 statuses" (User said 100)
    if (history.size() > 100) {
        history.pop_front();
    }
    
    pthread_mutex_unlock(&historyMutex);

    // --- END CRITICAL SECTION ---

    // 6. Unlock ALL Accounts (Decreasing Order)
    // We use reverse_iterator to go from End to Begin
    for (std::map<int, Account*>::reverse_iterator rit = accounts.rbegin(); rit != accounts.rend(); ++rit) {
        rit->second->unlockAccount(READER_MODE);
    }

    unlockBank(READER_MODE); 

    
}
// --------------------------------------------------------------------------
// Rollback Implementation
// --------------------------------------------------------------------------
void Bank::rollback(int atmId, int steps) {
    // 1. Acquire GLOBAL WRITE LOCK
    // This stops everything. ATM threads cannot read/write, Status thread cannot read.
    lockBank(WRITER_MODE); 

    pthread_mutex_lock(&historyMutex);

    // Validate Steps
    if (steps > history.size() || steps <= 0) {
        // Just return if invalid, or log specific error if required.
        // Assuming silently fail or do nothing if requested steps > history
        pthread_mutex_unlock(&historyMutex);
        unlockBank(WRITER_MODE);
        return;
    }

    // 2. "Rewind" history
    // We discard the most recent 'steps' states to find the target.
    // Logic: if steps=1, we remove current, and take the one before it.
    for (int i = 0; i < steps; ++i) {
        history.pop_back();
    }

    if (history.empty()) {
        // Rolled back to "Big Bang" (empty bank)
        for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
            delete it->second;
        }
        accounts.clear();
    } else {
        // 3. Restore State
        std::map<int, AccountSnapshot>& targetState = history.back();

        // A. DELETE accounts that exist NOW but did NOT exist THEN
        std::vector<int> idsToRemove;
        for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
            if (targetState.find(it->first) == targetState.end()) {
                idsToRemove.push_back(it->first);
            }
        }
        for (size_t i = 0; i < idsToRemove.size(); ++i) {
            int id = idsToRemove[i];
            delete accounts[id]; 
            accounts.erase(id);  
        }

        // B. UPDATE or CREATE accounts from target state
        for (std::map<int, AccountSnapshot>::iterator it = targetState.begin(); it != targetState.end(); ++it) {
            AccountSnapshot& data = it->second;

            if (accounts.find(data.id) != accounts.end()) {
                // Account exists: Use operator= to update data but KEEP MUTEX
                // We create a temp object to utilize the assignment operator
                Account temp(data.id, data.password, data.balanceILS, data.balanceUSD);
                *accounts[data.id] = temp; 
            } else {
                // Account was closed or didn't exist in current state: Create new
                Account* newAcc = new Account(data.id, data.password, data.balanceILS, data.balanceUSD);
                accounts[data.id] = newAcc;
            }
        }
    }

    // 4. Log Success
    // Format: <ATM ID>: Rollback to <iterations> bank iterations ago was completed successfully
    // Section F
    std::stringstream msg;
    msg << atmId << ": Rollback to " << steps << " bank iterations ago was completed successfully";
    LogFile::getInstance().write(msg.str());

    pthread_mutex_unlock(&historyMutex);
    unlockBank(WRITER_MODE);
}
//=====================End of Noa Added These Methods=========================


//=====================Additional Bank Implementation=========================

Bank::Bank()
    : isWorking(false),
      threadsStarted(false),
      commissionThread(),
      statusThread(),
      history(),
      historyMutex(),
      closeATMRequests(),
      closeATMMutex() {
    pthread_mutex_init(&historyMutex, NULL);
    pthread_mutex_init(&closeATMMutex, NULL);
}

Bank::~Bank() {
    stop();

    // Best-effort cleanup (main program might already own/deallocate these).
    lockBank(WRITER_MODE);
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        delete it->second;
    }
    accounts.clear();
    unlockBank(WRITER_MODE);

    pthread_mutex_destroy(&historyMutex);
    pthread_mutex_destroy(&closeATMMutex);
}

Bank& Bank::getInstance() {
    static Bank instance;
    return instance;
}

bool Bank::openAccount(int id, int pass, int initILS, int initUSD) {
    // Must lock internally (ATM does not lock before calling open)
    lockBank(WRITER_MODE);

    if (accounts.find(id) != accounts.end()) {
        unlockBank(WRITER_MODE);
        return false;
    }

    accounts[id] = new Account(id, pass, initILS, initUSD);
    unlockBank(WRITER_MODE);
    return true;
}

bool Bank::closeAccount(int id, int pass) {
    // IMPORTANT:
    // ATM.cpp already holds the Bank WRITER lock when calling closeAccount.
    // Do NOT lock the Bank here (or you'll deadlock).

    std::map<int, Account*>::iterator it = accounts.find(id);
    if (it == accounts.end()) {
        return false;
    }

    Account* acc = it->second;
    // Make sure no one modifies while we validate and delete.
    acc->lockAccount(WRITER_MODE);
    if (!acc->checkPassword(pass)) {
        acc->unlockAccount(WRITER_MODE);
        return false;
    }
    acc->unlockAccount(WRITER_MODE);

    delete acc;
    accounts.erase(it);
    return true;
}

Account* Bank::getAccount(int id) {
    // IMPORTANT:
    // ATM.cpp holds the Bank lock externally (READER/WRITER) before calling getAccount.
    // Do NOT lock the Bank here.
    std::map<int, Account*>::iterator it = accounts.find(id);
    return (it == accounts.end()) ? NULL : it->second;
}

void Bank::requestCloseATM(int requesterId, int targetId) {
    CloseATMRequest req;
    req.requesterId = requesterId;
    req.targetId = targetId;

    pthread_mutex_lock(&closeATMMutex);
    closeATMRequests.push(req);
    pthread_mutex_unlock(&closeATMMutex);
}

void Bank::handleCloseATMRequests() {
    // Drain the queue and close requested ATMs
    pthread_mutex_lock(&closeATMMutex);
    while (!closeATMRequests.empty()) {
        CloseATMRequest req = closeATMRequests.front();
        closeATMRequests.pop();
        pthread_mutex_unlock(&closeATMMutex);

        // Close is a best-effort action; ATM might not exist
        std::map<int, ATM*>::iterator it = atms.find(req.targetId);
        if (it != atms.end() && it->second != NULL) {
            it->second->close();
        }

        pthread_mutex_lock(&closeATMMutex);
    }
    pthread_mutex_unlock(&closeATMMutex);
}

void* Bank::commissionRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);
    unsigned int seed = static_cast<unsigned int>(std::time(NULL)) ^
                        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pthread_self()));

    while (bank->isWorking.load()) {
        // NEW REQUIREMENT: charge commission every 30ms
        usleep(30u * 1000u);
        if (!bank->isWorking.load()) {
            break;
        }

        bank->lockBank(READER_MODE);
        for (std::map<int, Account*>::iterator it = bank->accounts.begin(); it != bank->accounts.end(); ++it) {
            Account* acc = it->second;
            const int pct = (rand_r(&seed) % 5) + 1; // 1..5

            acc->lockAccount(WRITER_MODE);
            const int commissionILS = (acc->balanceILS * pct) / 100;
            const int commissionUSD = (acc->balanceUSD * pct) / 100;
            acc->balanceILS -= commissionILS;
            acc->balanceUSD -= commissionUSD;
            const int accId = acc->id;
            acc->unlockAccount(WRITER_MODE);

            std::stringstream msg;
            msg << "Bank: commissions of " << pct << " % were charged, bank gained "
                << commissionILS << " ILS and " << commissionUSD << " USD from account " << accId;
            LogFile::getInstance().write(msg.str());
        }
        bank->unlockBank(READER_MODE);
    }
    return NULL;
}

void* Bank::statusRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);

    while (bank->isWorking.load()) {
        usleep(510u * 1000u); // 0.5 seconds + 10 ms (unchanged)
        if (!bank->isWorking.load()) {
            break;
        }
        bank->takeSnapshot();
        bank->handleCloseATMRequests();
    }
    return NULL;
}

void Bank::run() {
    if (threadsStarted) {
        return;
    }
    isWorking.store(true);

    if (pthread_create(&commissionThread, NULL, Bank::commissionRoutine, this) == 0 &&
        pthread_create(&statusThread, NULL, Bank::statusRoutine, this) == 0) {
        threadsStarted = true;
    } else {
        // If thread creation fails, stop gracefully
        isWorking.store(false);
        threadsStarted = false;
    }
}

void Bank::stop() {
    if (!threadsStarted) {
        isWorking.store(false);
        return;
    }

    isWorking.store(false);
    pthread_join(commissionThread, NULL);
    pthread_join(statusThread, NULL);
    threadsStarted = false;
}


