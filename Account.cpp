#include "Account.h"
#include "LogFile.h"   // For LogFile::getInstance().write

#include <cmath>        // For std::round
#include <sstream>      // For std::stringstream

// --------------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------------
Account::Account(int id, int password, int initILS, int initUSD)
    // IMPORTANT: Member initialization order follows the declaration order in Account.h.
    // This avoids -Wreorder (treated as error with -Werror).
    : accountLock(),                // <--- Explicitly call LockRW constructor
      id(id),                       // Initialize integer id
      password(password),           // Initialize integer password
      balanceILS(initILS),          // Initialize integer balanceILS
      balanceUSD(initUSD)           // Initialize integer balanceUSD
{
    // The body is empty because everything was done in the initializer list.
}

// --------------------------------------------------------------------------
// Copy Constructor
// --------------------------------------------------------------------------
Account::Account(const Account& other) 
    // IMPORTANT: Member initialization order follows the declaration order in Account.h.
    : accountLock(),                  // <--- Explicitly create a NEW, fresh lock
      id(other.id),                   // Copy the ID
      password(other.password),       // Copy the Password
      balanceILS(other.balanceILS),   // Copy the Balance
      balanceUSD(other.balanceUSD)    // Copy the Balance
{
    // Body is empty

    // CRITICAL EXPLANATION:
    // We explicitly COPY the data (id, password, balance), but we do NOT copy 
    // the 'other.accountLock'.
    //
    // 1. Semantics: A new account is a distinct entity. Even if it has the same data, 
    //    it shouldn't share the synchronization state of the old one.
    // 2. Technical: 'pthread_mutex_t' and 'pthread_cond_t' (inside LockRW) cannot 
    //    be copied safely. Copying a locked mutex results in undefined behavior.
    //
    // Therefore, 'this->accountLock' is Explicitly-Constructed (fresh and unlocked).
}

// --------------------------------------------------------------------------
// Assignment Operator
// --------------------------------------------------------------------------
Account& Account::operator=(const Account& other) {
    // 1. Check for self-assignment (e.g., acc = acc)
    if (this == &other) {
        return *this;
    }

    // 2. Copy the data
    // We are changing the identity/data of this account to match 'other'.
    id = other.id;
    password = other.password;
    balanceILS = other.balanceILS;
    balanceUSD = other.balanceUSD;

    // CRITICAL EXPLANATION:
    // We DO NOT assign 'accountLock = other.accountLock'.
    // 
    // If threads are currently waiting on 'this->accountLock', overwriting the lock
    // underneath them would cause a crash or deadlock. 
    // We keep the EXISTING lock of this object, protecting the NEW data we just copied.
    
    return *this;
}

// --------------------------------------------------------------------------
// Destructor
// --------------------------------------------------------------------------
Account::~Account() {
    // No manual cleanup needed for int members.
    // accountLock destructor will be called automatically, destroying the mutexes.
}

// --------------------------------------------------------------------------
// checkPassword
// --------------------------------------------------------------------------
bool Account::checkPassword(int pwd) {
    // Since the variables are public, the ATM will typically hold a lock 
    // before calling this to ensure the password doesn't change mid-check.
    // However, since passwords don't change in this assignment, 
    // this read is generally thread-safe even without a lock.
    return (this->password == pwd);
}

// --------------------------------------------------------------------------
// Bank Commission
// --------------------------------------------------------------------------
void Account::takeCommission(double percentage) {
    // 1. Acquire Writer Lock
    // We are changing the balance, so we need exclusive access.
    lockAccount(WRITER_MODE);

    // 2. Calculate Commission
    // "The bank charges a commission... from the current balance" [cite: 75]
    // Note: Using round() as is standard for currency in these types of exercises
    int commissionILS = (int)std::round(balanceILS * (percentage / 100.0));
    int commissionUSD = (int)std::round(balanceUSD * (percentage / 100.0));

    // 3. Deduct from Balance
    balanceILS -= commissionILS;
    balanceUSD -= commissionUSD;

    // 4. Log the operation internally
    // Format: "Bank: commissions of <%> % were charged, bank gained <ils> ILS and <usd> USD from account <id>" 
    std::stringstream ss;
    ss << "Bank: commissions of " << (int)percentage << " % were charged, "
       << "bank gained " << commissionILS << " ILS and " << commissionUSD << " USD "
       << "from account " << id;
    
    LogFile::getInstance().write(ss.str());

    // 5. Release Lock
    unlockAccount(WRITER_MODE);
    
    // Optional: If you decide later you DO need to track the Bank's total profits 
    // (per requirement ), you could return a pair<int, int> here 
    // containing {commissionILS, commissionUSD} and let the Bank add it to its vault.
}


// --------------------------------------------------------------------------
// Lock Wrappers
// --------------------------------------------------------------------------
void Account::lockAccount(bool writeMode) {
    if (writeMode == WRITER_MODE) {
        accountLock.writeEnter();
    } else {
        accountLock.readEnter();
    }
}

void Account::unlockAccount(bool writeMode) {
    if (writeMode == WRITER_MODE) {
        accountLock.writeExit();
    } else {
        accountLock.readExit();
    }
}
