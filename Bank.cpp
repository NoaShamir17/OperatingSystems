#include "Bank.h"
#include "LogFile.h"
#include <iostream>
#include <unistd.h>
#include <sstream>

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



// Ensure you initialize the mutex in your Constructor (Bank::Bank())
// pthread_mutex_init(&historyMutex, NULL); 
// And destroy it in Destructor ~Bank()

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

