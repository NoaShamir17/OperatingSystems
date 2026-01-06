#include "Bank.h"

#include "ATM.h"       // needed for ATM::close / ATM::isActive
#include "LogFile.h"

#include <unistd.h>     // usleep
#include <sstream>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <vector>

// ---------------------------
// Internal helpers (Bank.cpp)
// ---------------------------

namespace {

// Investment: argument pack for detached thread
struct InvestmentThreadArgs {
    int atmId;
    int accountId;
    int amount;
    std::string currency;
    int timeMillis;
};

void* investmentThreadRoutine(void* arg) {
    InvestmentThreadArgs* data = static_cast<InvestmentThreadArgs*>(arg);
    const int atmId = data->atmId;
    const int accountId = data->accountId;
    const int amount = data->amount;
    const std::string currency = data->currency;
    const int timeMillis = data->timeMillis;
    delete data;

    usleep(static_cast<useconds_t>(timeMillis) * 1000u);

    // 3% interest per 10 milisecond (timeMillis is in milliseconds)
    const double factor = std::pow(1.03, static_cast<double>(timeMillis) / 10);
    const int finalAmount = static_cast<int>(std::round(amount * factor));

    Bank& bank = Bank::getInstance();
    bank.lockBank(READER_MODE);
    Account* acc = bank.getAccount(accountId);
    if (acc != NULL) {
        acc->lockAccount(WRITER_MODE);
        if (currency == "ILS") {
            acc->balanceILS += finalAmount;
        } else {
            acc->balanceUSD += finalAmount;
        }
        acc->unlockAccount(WRITER_MODE);
    }
    bank.unlockBank(READER_MODE);

    (void)atmId; // unused (kept for potential future logging)
    return NULL;
}

// Trim helper
static std::string trimRight(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    if (end == std::string::npos) {
        return std::string();
    }
    return s.substr(0, end + 1);
}

} // namespace


// ---------------------------
// Singleton
// ---------------------------

Bank& Bank::getInstance() {
    static Bank instance;
    return instance;
}

Bank::Bank()
    : isWorking(false),
      commissionThread(),
      statusThread(),
      closeATMQueue(),
      //rollbackQueue(),
      vipQueue(),
      vipThreads(),
      vipThreadCount(0),
      vipSequenceCounter(0) {

    pthread_mutex_init(&historyMutex, NULL);
    pthread_mutex_init(&closeATMMutex, NULL);
    pthread_mutex_init(&rollbackMutex, NULL);
    pthread_mutex_init(&vipMutex, NULL);
    pthread_cond_init(&vipCond, NULL);
}

Bank::~Bank() {
    stop();

    // Best-effort cleanup (main flow typically exits anyway)
    lockBank(WRITER_MODE);
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        delete it->second;
    }
    accounts.clear();
    unlockBank(WRITER_MODE);

    pthread_cond_destroy(&vipCond);
    pthread_mutex_destroy(&vipMutex);
    pthread_mutex_destroy(&rollbackMutex);
    pthread_mutex_destroy(&closeATMMutex);
    pthread_mutex_destroy(&historyMutex);
}


// ---------------------------
// Lock management
// ---------------------------

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


// ---------------------------
// Account management
// ---------------------------

bool Bank::openAccount(int id, int pass, int initILS, int initUSD) {
    // Per course note: openAccount must lock internally (ATM does not lock before calling).
    lockBank(WRITER_MODE);
    if (accounts.find(id) != accounts.end()) {
        unlockBank(WRITER_MODE);
        return false;
    }

    Account* acc = new Account(id, pass, initILS, initUSD);
    accounts[id] = acc;
    unlockBank(WRITER_MODE);
    return true;
}

bool Bank::closeAccount(int id, int pass) {
    // IMPORTANT: Caller is expected to hold Bank WRITER lock externally (ATM.cpp already does).
    std::map<int, Account*>::iterator it = accounts.find(id);
    if (it == accounts.end()) {
        return false;
    }
    Account* acc = it->second;    

    delete acc;
    accounts.erase(it);
    return true;
}

Account* Bank::getAccount(int id) {
    std::map<int, Account*>::iterator it = accounts.find(id);
    if (it == accounts.end()) {
        return NULL;
    }
    return it->second;
}


// ---------------------------
// ATM / Bank coordination
// ---------------------------

void Bank::requestCloseATM(int requesterId, int targetId) {
    CloseATMRequest req;
    req.requesterId = requesterId;
    req.targetId = targetId;

    pthread_mutex_lock(&closeATMMutex);
    closeATMQueue.push(req);
    pthread_mutex_unlock(&closeATMMutex);
}

// void Bank::requestRollback(int requesterId, int iterations) {
//     RollbackRequest req;
//     req.requesterId = requesterId;
//     req.iterations = iterations;

//     pthread_mutex_lock(&rollbackMutex);
//     rollbackQueue.push(req);
//     pthread_mutex_unlock(&rollbackMutex);
// }

void Bank::addVIPRequest(int requesterATM, const std::string& fullLine) {
    // Parse trailing "VIP=X" token (appears as the last word with one space).
    int priority = 0;
    std::string line = trimRight(fullLine);

    const std::string key = "VIP=";
    const size_t pos = line.rfind(key);
    if (pos != std::string::npos) {
        // Ensure it is the last token
        size_t spacePos = line.rfind(' ', pos);
        if (spacePos != std::string::npos) {
            const std::string prioStr = line.substr(pos + key.size());
            priority = std::atoi(prioStr.c_str());
            line = trimRight(line.substr(0, spacePos));
        }
    }
    if (priority < 1) priority = 1;
    if (priority > 100) priority = 100;

    VIPRequest req;
    req.priority = priority;
    req.requesterATM = requesterATM;
    req.commandLine = line;

    pthread_mutex_lock(&vipMutex);
    req.sequence = vipSequenceCounter++;
    vipQueue.push(req);
    pthread_cond_signal(&vipCond);
    pthread_mutex_unlock(&vipMutex);
}


// ---------------------------
// Background threads control
// ---------------------------

void Bank::run(int numberOfVIPThreads) {
    if (isWorking.load()) {
        return;
    }

    isWorking.store(true);
    vipThreadCount = std::max(0, numberOfVIPThreads);
    vipThreads.clear();
    vipThreads.resize(static_cast<size_t>(vipThreadCount));

    // Start VIP consumer threads
    for (int i = 0; i < vipThreadCount; ++i) {
        if (pthread_create(&vipThreads[static_cast<size_t>(i)], NULL, Bank::vipRoutine, this) != 0) {
            // Best effort: continue creating others
        }
    }

    pthread_create(&commissionThread, NULL, Bank::commissionRoutine, this);
    pthread_create(&statusThread, NULL, Bank::statusRoutine, this);
}

void Bank::stop() {
    if (!isWorking.load()) {
        return;
    }

    isWorking.store(false);

    // Wake VIP threads waiting on the queue
    pthread_mutex_lock(&vipMutex);
    pthread_cond_broadcast(&vipCond);
    pthread_mutex_unlock(&vipMutex);

    // Join background threads (ignore errors)
    pthread_join(statusThread, NULL);
    pthread_join(commissionThread, NULL);

    for (size_t i = 0; i < vipThreads.size(); ++i) {
        pthread_join(vipThreads[i], NULL);
    }
    vipThreads.clear();
}


// ---------------------------
// Snapshot (atomic view)
// ---------------------------

void Bank::takeSnapshot(){//bool saveToHistory) {
    // 1) Lock bank for reading (stabilize map iteration)
    lockBank(READER_MODE);

    // 2) Lock ALL accounts for reading (freeze state for consistent snapshot)
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        it->second->lockAccount(READER_MODE);
    }

    std::map<int, AccountSnapshot> currentSnapshot;

    // 3) Print
    printf("\033[2J");
    printf("\033[1;1H");
    printf("Current Bank Status\n");

    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        Account* acc = it->second;

        AccountSnapshot snap;
        snap.id = acc->id;
        snap.password = acc->password;
        snap.balanceILS = acc->balanceILS;
        snap.balanceUSD = acc->balanceUSD;
        currentSnapshot[acc->id] = snap;

        printf("Account %d: Balance - %d ILS %d USD, Account Password - %d\n",
               acc->id, acc->balanceILS, acc->balanceUSD, acc->password);
    }

    // IMPORTANT: When stdout is redirected (e.g., during automated tests),
    // it becomes fully-buffered. We must flush so that the status text is
    // visible even if the process is terminated by a timeout.
    fflush(stdout);

    // 4) Push to history (max 100) - optionally
    // if (saveToHistory) {
    //     pthread_mutex_lock(&historyMutex);
    //     history.push_back(currentSnapshot);
    //     while (history.size() > 100u) {
    //         history.pop_front();
    //     }
    //     pthread_mutex_unlock(&historyMutex);
    // }

    pthread_mutex_lock(&historyMutex);
    history.push_back(currentSnapshot);
    while (history.size() > 100u) {
        history.pop_front();
    }
    pthread_mutex_unlock(&historyMutex);

    // 5) Unlock ALL accounts (reverse order)
    for (std::map<int, Account*>::reverse_iterator rit = accounts.rbegin(); rit != accounts.rend(); ++rit) {
        rit->second->unlockAccount(READER_MODE);
    }

    unlockBank(READER_MODE);
}


// ---------------------------
// Rollback (executor)
// ---------------------------

void Bank::rollback(int atmId, int steps) {
    lockBank(WRITER_MODE);
    pthread_mutex_lock(&historyMutex);

    const size_t hsz = history.size();
    if (steps <= 0 || hsz == 0 || static_cast<size_t>(steps) >= hsz) {
        // Not enough history to rollback (need at least one older snapshot)
        pthread_mutex_unlock(&historyMutex);
        unlockBank(WRITER_MODE);
        return;
    }

    // Remove the last 'steps' snapshots, then restore to new last snapshot
    for (int i = 0; i < steps; ++i) {
        history.pop_back();
    }

    const std::map<int, AccountSnapshot>& target = history.back();

    // Remove accounts that are not in target
    std::vector<int> idsToRemove;
    for (std::map<int, Account*>::iterator it = accounts.begin(); it != accounts.end(); ++it) {
        if (target.find(it->first) == target.end()) {
            idsToRemove.push_back(it->first);
        }
    }
    for (size_t i = 0; i < idsToRemove.size(); ++i) {
        const int id = idsToRemove[i];
        std::map<int, Account*>::iterator it = accounts.find(id);
        if (it != accounts.end()) {
            delete it->second;
            accounts.erase(it);
        }
    }

    // Update/create accounts from target
    for (std::map<int, AccountSnapshot>::const_iterator it = target.begin(); it != target.end(); ++it) {
        const AccountSnapshot& snap = it->second;
        std::map<int, Account*>::iterator cur = accounts.find(snap.id);
        if (cur == accounts.end()) {
            accounts[snap.id] = new Account(snap.id, snap.password, snap.balanceILS, snap.balanceUSD);
        } else {
            Account* acc = cur->second;
            acc->id = snap.id;
            acc->password = snap.password;
            acc->balanceILS = snap.balanceILS;
            acc->balanceUSD = snap.balanceUSD;
        }
    }

    // Log success
    std::stringstream msg;
    msg << atmId << ": Rollback to " << steps << " bank iterations ago was completed successfully";
    LogFile::getInstance().write(msg.str());

    pthread_mutex_unlock(&historyMutex);
    unlockBank(WRITER_MODE);
}


// ---------------------------
// Status thread: handle queues
// ---------------------------

void Bank::handleCloseATMRequests() {
    std::vector<CloseATMRequest> reqs;

    pthread_mutex_lock(&closeATMMutex);
    while (!closeATMQueue.empty()) {
        reqs.push_back(closeATMQueue.front());
        closeATMQueue.pop();
    }
    pthread_mutex_unlock(&closeATMMutex);

    for (size_t i = 0; i < reqs.size(); ++i) {
        const int sourceId = reqs[i].requesterId;
        const int targetId = reqs[i].targetId;

        std::map<int, ATM*>::iterator it = atms.find(targetId);
        if (it == atms.end()) {
            std::stringstream err;
            err << "Error " << sourceId << ": Your transaction failed - ATM ID " << targetId << " does not exist";
            LogFile::getInstance().write(err.str());
            continue;
        }

        ATM* target = it->second;
        if (!target->isActive()) {
            std::stringstream err;
            err << "Error " << sourceId << ": Your close operation failed - ATM ID " << targetId << " is already in a closed state";
            LogFile::getInstance().write(err.str());
            continue;
        }

        target->close();
        std::stringstream ok;
        ok << "Bank: ATM " << sourceId << " closed " << targetId << " successfully";
        LogFile::getInstance().write(ok.str());
    }
}

// void Bank::handleRollbackRequests() {
//     std::vector<RollbackRequest> reqs;

//     pthread_mutex_lock(&rollbackMutex);
//     while (!rollbackQueue.empty()) {
//         reqs.push_back(rollbackQueue.front());
//         rollbackQueue.pop();
//     }
//     pthread_mutex_unlock(&rollbackMutex);

//     for (size_t i = 0; i < reqs.size(); ++i) {
//         rollback(reqs[i].requesterId, reqs[i].iterations);
//     }
// }


// ---------------------------
// VIP thread: consumer
// ---------------------------

void* Bank::vipRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);

    while (true) {
        pthread_mutex_lock(&bank->vipMutex);
        while (bank->vipQueue.empty() && bank->isWorking.load()) {
            pthread_cond_wait(&bank->vipCond, &bank->vipMutex);
        }

        if (!bank->isWorking.load() && bank->vipQueue.empty()) {
            pthread_mutex_unlock(&bank->vipMutex);
            break;
        }

        if (bank->vipQueue.size() == 1u) {
            // Re-check stop condition after yielding
            while (bank->vipQueue.empty() && bank->isWorking.load()) {
                pthread_cond_wait(&bank->vipCond, &bank->vipMutex);
            }
            if (!bank->isWorking.load() && bank->vipQueue.empty()) {
                pthread_mutex_unlock(&bank->vipMutex);
                break;
            }
        }

        VIPRequest req = bank->vipQueue.top();
        bank->vipQueue.pop();
        pthread_mutex_unlock(&bank->vipMutex);

        bank->executeCommandLine(req.requesterATM, req.commandLine);
    }

    return NULL;
}


// ---------------------------
// Command execution for VIP
// ---------------------------

void Bank::executeCommandLine(int atmId, const std::string& line) {
    std::stringstream ss(line);
    char cmd;
    ss >> cmd;

    int accountId, password, amount, targetId, sleepTime;
    std::string currencyStr;

    switch (cmd) {
        case 'O': {
            int initILS, initUSD;
            ss >> accountId >> password >> initILS >> initUSD;
            if (openAccount(accountId, password, initILS, initUSD)) {
                std::stringstream msg;
                msg << atmId << ": New account id is " << accountId << " with password " << password
                    << " and initial balance " << initILS << " ILS and " << initUSD << " USD";
                LogFile::getInstance().write(msg.str());
            } else {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account with the same id exists";
                LogFile::getInstance().write(msg.str());
            }
            return;
        }

        case 'D': {
            ss >> accountId >> password >> amount >> currencyStr;
            const bool isILS = (currencyStr == "ILS");

            lockBank(READER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            acc->lockAccount(WRITER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            if (isILS) acc->balanceILS += amount;
            else       acc->balanceUSD += amount;

            std::stringstream msg;
            msg << atmId << ": Account " << accountId << " new balance is " << acc->balanceILS
                << " ILS and " << acc->balanceUSD << " USD after " << amount << " " << currencyStr << " was deposited";
            LogFile::getInstance().write(msg.str());

            acc->unlockAccount(WRITER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'W': {
            ss >> accountId >> password >> amount >> currencyStr;
            const bool isILS = (currencyStr == "ILS");

            lockBank(READER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            acc->lockAccount(WRITER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            const int currentBalance = isILS ? acc->balanceILS : acc->balanceUSD;
            if (currentBalance < amount) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId
                    << " balance is " << acc->balanceILS << " ILS and " << acc->balanceUSD
                    << " USD is lower than " << amount << " " << currencyStr;
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            if (isILS) acc->balanceILS -= amount;
            else       acc->balanceUSD -= amount;

            std::stringstream msg;
            msg << atmId << ": Account " << accountId << " new balance is " << acc->balanceILS
                << " ILS and " << acc->balanceUSD << " USD after " << amount << " " << currencyStr << " was withdrawn";
            LogFile::getInstance().write(msg.str());

            acc->unlockAccount(WRITER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'B': {
            ss >> accountId >> password;

            lockBank(READER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            acc->lockAccount(READER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(READER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            std::stringstream msg;
            msg << atmId << ": Account " << accountId << " balance is " << acc->balanceILS
                << " ILS and " << acc->balanceUSD << " USD";
            LogFile::getInstance().write(msg.str());

            acc->unlockAccount(READER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'T': {
            ss >> accountId >> password >> targetId >> amount >> currencyStr;
            const bool isILS = (currencyStr == "ILS");

            lockBank(READER_MODE);
            Account* src = getAccount(accountId);
            Account* dst = getAccount(targetId);

            if (!src) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }
            if (!dst) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << targetId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            Account* first = (src->id < dst->id) ? src : dst;
            Account* second = (src->id < dst->id) ? dst : src;
            first->lockAccount(WRITER_MODE);
            second->lockAccount(WRITER_MODE);

            if (!src->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                second->unlockAccount(WRITER_MODE);
                first->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            const int currentBalance = isILS ? src->balanceILS : src->balanceUSD;
            if (currentBalance < amount) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - balance of account id " << accountId 
                     << " is lower than " << amount << " " << currencyStr;

                LogFile::getInstance().write(msg.str());
                second->unlockAccount(WRITER_MODE);
                first->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            if (isILS) {
                src->balanceILS -= amount;
                dst->balanceILS += amount;
            } else {
                src->balanceUSD -= amount;
                dst->balanceUSD += amount;
            }
            

            // <ATM ID>: Transfer <amount> <currency> from account <source account> to account <target account>
            // new account balance is <source balance ILS> ILS and < source balance USD> USD new target account
            // balance is <target balance ILS> ILS and <target balance USD> USD
            std::stringstream msg;
            msg << atmId << ": Transfer " << amount << " " << currencyStr
                << " from account " << accountId << " to account " << targetId
                << " new account balance is " << src->balanceILS << " ILS and " << src->balanceUSD
                << " USD new target account balance is " << dst->balanceILS << " ILS and " << dst->balanceUSD << " USD";
            LogFile::getInstance().write(msg.str());

            second->unlockAccount(WRITER_MODE);
            first->unlockAccount(WRITER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'Q': {
            ss >> accountId >> password;

            lockBank(WRITER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(WRITER_MODE);
                return;
            }

            acc->lockAccount(READER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(READER_MODE);
                unlockBank(WRITER_MODE);
                return;
            }
            const int savedILS = acc->balanceILS;
            const int savedUSD = acc->balanceUSD;
            acc->unlockAccount(READER_MODE);

            if (closeAccount(accountId, password)) {
                std::stringstream msg;
                msg << atmId << ": Account " << accountId << " is now closed. Balance was "
                    << savedILS << " ILS and " << savedUSD << " USD";
                LogFile::getInstance().write(msg.str());
            } else {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
            }

            unlockBank(WRITER_MODE);
            return;
        }

        case 'C': {
            ss >> targetId;
            requestCloseATM(atmId, targetId);
            return;
        }

        case 'S': {
            ss >> sleepTime;
            std::stringstream msg;
            msg << atmId << ": Currently on a scheduled break. Service will resume within " << sleepTime << " ms.";
            LogFile::getInstance().write(msg.str());
            usleep(static_cast<useconds_t>(sleepTime) * 1000u);
            return;
        }

        case 'X': {
            std::string sourceCurrency, toWord, targetCurrency;
            ss >> accountId >> password >> sourceCurrency >> toWord >> targetCurrency >> amount;

            lockBank(READER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            acc->lockAccount(WRITER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            bool enoughFunds = false;
            if (sourceCurrency == "ILS") {
                if (acc->balanceILS >= amount) {
                    enoughFunds = true;
                    acc->balanceILS -= amount;
                    acc->balanceUSD += (amount / DOLLAR_TO_ILS_RATE);
                }
            } else {
                if (acc->balanceUSD >= amount) {
                    enoughFunds = true;
                    acc->balanceUSD -= amount;
                    acc->balanceILS += (amount * DOLLAR_TO_ILS_RATE);
                }
            }

            if (!enoughFunds) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId
                    << " balance is " << acc->balanceILS << " ILS and " << acc->balanceUSD
                    << " USD is lower than " << amount << " " << sourceCurrency;
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            std::stringstream msg;
            msg << atmId << ": Account " << accountId << " new balance is " << acc->balanceILS
                << " ILS and " << acc->balanceUSD << " USD after " << amount << " " << sourceCurrency
                << " was exchanged";
            LogFile::getInstance().write(msg.str());

            acc->unlockAccount(WRITER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'I': {
            int timeMillis;
            ss >> accountId >> password >> amount >> currencyStr >> timeMillis;

            lockBank(READER_MODE);
            Account* acc = getAccount(accountId);
            if (!acc) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId << " does not exist";
                LogFile::getInstance().write(msg.str());
                unlockBank(READER_MODE);
                return;
            }

            acc->lockAccount(WRITER_MODE);
            if (!acc->checkPassword(password)) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - password for account id " << accountId << " is incorrect";
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            bool hasFunds = false;
            if (currencyStr == "ILS") {
                if (acc->balanceILS >= amount) {
                    acc->balanceILS -= amount;
                    hasFunds = true;
                }
            } else {
                if (acc->balanceUSD >= amount) {
                    acc->balanceUSD -= amount;
                    hasFunds = true;
                }
            }

            if (!hasFunds) {
                std::stringstream msg;
                msg << "Error " << atmId << ": Your transaction failed - account id " << accountId
                    << " balance is " << acc->balanceILS << " ILS and " << acc->balanceUSD
                    << " USD is lower than " << amount << " " << currencyStr;
                LogFile::getInstance().write(msg.str());
                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }

            InvestmentThreadArgs* args = new InvestmentThreadArgs;
            args->atmId = atmId;
            args->accountId = accountId;
            args->amount = amount;
            args->currency = currencyStr;
            args->timeMillis = timeMillis;

            pthread_t t;
            if (pthread_create(&t, NULL, investmentThreadRoutine, args) != 0) {
                // Refund
                if (currencyStr == "ILS") acc->balanceILS += amount;
                else acc->balanceUSD += amount;

                delete args;
                std::stringstream msg;
                msg << "Error " << atmId << ": System error - failed to create investment thread";
                LogFile::getInstance().write(msg.str());

                acc->unlockAccount(WRITER_MODE);
                unlockBank(READER_MODE);
                return;
            }
            pthread_detach(t);

            acc->unlockAccount(WRITER_MODE);
            unlockBank(READER_MODE);
            return;
        }

        case 'R': {
            int iterations;
            ss >> iterations;
            rollback(atmId, iterations);
            return;
        }

        default:
            return;
    }
}


// ---------------------------
// Thread routines
// ---------------------------

void* Bank::commissionRoutine(void* arg) {
    Bank* bank = static_cast<Bank*>(arg);
    unsigned int seed = static_cast<unsigned int>(std::time(NULL)) ^ static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pthread_self()));

    while (bank->isWorking.load()) {
        usleep(30u * 1000u); // 30 ms
        if (!bank->isWorking.load()) {
            break;
        }
        const int pct = (rand_r(&seed) % 5) + 1; // 1..5

        bank->lockBank(READER_MODE);
        for (std::map<int, Account*>::iterator it = bank->accounts.begin(); it != bank->accounts.end(); ++it) {
            Account* acc = it->second;

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
        // If a rollback is pending, we still print the status but we do NOT
        // push a new snapshot before performing the rollback. This keeps the
        // meaning of "R k" as rolling back k previously completed bank iterations.
        // bool rollbackPending = false;
        // pthread_mutex_lock(&bank->rollbackMutex);
        // rollbackPending = !bank->rollbackQueue.empty();
        // pthread_mutex_unlock(&bank->rollbackMutex);

        bank->takeSnapshot();//!rollbackPending);
        bank->handleCloseATMRequests();
        bank->handleRollbackRequests();

        // Print first, then sleep. This guarantees that we have an initial
        // snapshot early in the run (helps rollback correctness and tests).
        usleep(10u * 1000u); // 10 ms
    }
    return NULL;
}
