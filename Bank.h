#ifndef BANK_H
#define BANK_H

#include <map>
#include <list>
#include <vector>
#include <queue>
#include <string>
#include <cstdint>
#include <atomic>
#include <pthread.h>
#include <algorithm>
#include "LockRW.h"
#include "Account.h"

// Forward declaration (avoids include cycle with ATM.h)
class ATM;

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

    std::atomic<bool> isWorking; // Flag for threads (atomic to avoid data races)
    pthread_t commissionThread;
    pthread_t statusThread; //prints status periodically, updates status history, handles ATM close requests

    // --- History Management ---
    // List of maps. Each map is a "status" of the bank at a point in time.
    std::list<std::map<int, AccountSnapshot> > history; 
    pthread_mutex_t historyMutex;

    // --- ATM Close Requests (handled by status thread) ---
    struct CloseATMRequest {
        int requesterId;
        int targetId;
    };
    std::queue<CloseATMRequest> closeATMQueue;
    pthread_mutex_t closeATMMutex;

    // // --- Rollback Requests (handled by status thread) ---
    // struct RollbackRequest {
    //     int requesterId;
    //     int iterations;
    // };
    // std::queue<RollbackRequest> rollbackQueue;
    // pthread_mutex_t rollbackMutex;

    // --- VIP Requests (producer-consumer, priority) ---
    struct VIPRequest {
        int priority;            // 1..100 (higher = earlier)
        std::uint64_t sequence;  // FIFO tie-breaker
        int requesterATM;        // original ATM id (for logging)
        std::string commandLine; // command line without trailing VIP=X
    };
    struct VIPCompare {
        bool operator()(const VIPRequest& a, const VIPRequest& b) const {
            if (a.priority != b.priority) {
                return a.priority < b.priority; // max-heap by priority
            }
            return a.sequence > b.sequence;     // earlier sequence first
        }
    };

    std::priority_queue<VIPRequest, std::vector<VIPRequest>, VIPCompare> vipQueue;
    pthread_mutex_t vipMutex;
    pthread_cond_t vipCond;
    std::vector<pthread_t> vipThreads;
    int vipThreadCount;
    std::uint64_t vipSequenceCounter;

    // Helpers
    void handleCloseATMRequests();
    void handleRollbackRequests();
    static void* vipRoutine(void* arg);
    void executeCommandLine(int atmId, const std::string& line);

    // Internal helper to print status and optionally store a snapshot in history.
    // When a rollback is pending, we print but avoid pushing a new snapshot so that
    // "R k" rolls back relative to the last completed iteration.
    void takeSnapshot(bool saveToHistory = true);

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

    // Rollback (internal executor): locks the whole bank, restores state
    // Note: per assignment, rollback is executed by the status thread *after* printing status.
    void rollback(int atmId, int steps);

    // Rollback request: enqueue for execution after next status print.
    void requestRollback(int requesterId, int iterations);

    // VIP: enqueue a VIP command line for processing by VIP consumer threads.
    void addVIPRequest(int requesterATM, const std::string& fullLine);

    //----------------Thread Routines--------------------------

    // Bank Commission Loop (Thread function)
    static void* commissionRoutine(void* arg); // [cite: 74]
    
    // Status Printer Loop (Thread function)
    static void* statusRoutine(void* arg);

    //-----------------System control--------------------------
    void run(int numberOfVIPThreads);
    void stop();
};




#endif
