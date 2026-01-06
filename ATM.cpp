#include "ATM.h"
#include "Bank.h"
#include "Account.h"
#include "LogFile.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>  // For usleep
#include <algorithm> // For std::min, std::max
#include <cmath>     // For std::pow, std::round

// --------------------------------------------------------------------------
// Constructor & Destructor
// --------------------------------------------------------------------------
ATM::ATM(int id, const std::string& filePath) 
    : id(id), inputFilePath(filePath), active(true) {
}

ATM::~ATM() {
    // Thread joining is typically handled by the Bank or Main function
}

// --------------------------------------------------------------------------
// Lifecycle Management
// --------------------------------------------------------------------------
void ATM::close() {
    active = false; // The run loop will see this and exit
}

bool ATM::isActive() const {
    return active;
}

void* ATM::startRoutine(void* arg) {
    ATM* atm = (ATM*)arg;
    atm->run();
    return NULL;
}

// --------------------------------------------------------------------------
// Main Execution Loop
// --------------------------------------------------------------------------
void ATM::run() {
    std::ifstream file(inputFilePath.c_str());
    if (!file.is_open()) {
        std::cerr << "Bank error: illegal arguments" << std::endl; 
        return;
    }

    std::string line;
    while (active && std::getline(file, line)) {
        
        // 1. Check if we were closed while reading
        if (!active) break;
        if (line.empty()) continue;

        // 2. Check for VIP Command
        // "ATM... will write it to a special data structure... Bank will create special threads"
        bool isVIP = (line.find("VIP") != std::string::npos);

        if (isVIP) {
            // Hand off the entire command line to the Bank's VIP handler
            // (Assuming Bank has a method addVIPRequest(std::string))
            // Bank::getInstance().addVIPRequest(line); 
            Bank::getInstance().addVIPRequest(id, line);
            continue; // Do not execute locally
        }

        // 3. Execute Standard Command
        processCommand(line);

    }
    
    file.close();
    active = false; // Mark as finished
}

// --------------------------------------------------------------------------
// Command Processing
// --------------------------------------------------------------------------
bool ATM::processCommand(const std::string& line) {
    std::stringstream ss(line);
    char cmd;
    ss >> cmd;

    int accountId, password, amount, targetId, sleepTime;
    std::string currencyStr; 

    switch (cmd) {
        
        // -------------------------------------------------------
        // O: Open Account
        // Format: O <id> <pass> <initILS> <initUSD>
        // -------------------------------------------------------
        case 'O': { 
            int initILS, initUSD;
            ss >> accountId >> password >> initILS >> initUSD;
            
            // Bank handles the Map Write Lock internally
            if (Bank::getInstance().openAccount(accountId, password, initILS, initUSD)) {
                std::stringstream msg;
                msg << id << ": New account id is " << accountId << " with password " << password 
                    << " and initial balance " << initILS << " ILS and " << initUSD << " USD";
                logSuccess(msg.str());
                return true;
            } else {
                std::stringstream msg;
                msg << "Error " << id << ": Your transaction failed - account with the same id exists";
                logError(msg.str());
                return false;
            }
        }

        // -------------------------------------------------------
        // D: Deposit
        // Format: D <id> <pass> <amount> <currency>
        // -------------------------------------------------------
        case 'D': { 
            ss >> accountId >> password >> amount >> currencyStr;
            bool isILS = (currencyStr == "ILS");
            //lock the Bank for reading
            Bank::getInstance().lockBank(READER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            // Lock for writing (modifying balance)
            acc->lockAccount(WRITER_MODE);

            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
                Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }
            
            if (isILS) acc->balanceILS += amount;
            else       acc->balanceUSD += amount;
            
            int balILS = acc->balanceILS;
            int balUSD = acc->balanceUSD;


            std::stringstream msg;
            msg << id << ": Account " << accountId << " new balance is " << balILS 
                << " ILS and " << balUSD << " USD after " << amount << " " << currencyStr << " was deposited";
            logSuccess(msg.str());
            acc->unlockAccount(WRITER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);

            return true;
        }

        // -------------------------------------------------------
        // W: Withdraw
        // Format: W <id> <pass> <amount> <currency>
        // -------------------------------------------------------
        case 'W': { 
            ss >> accountId >> password >> amount >> currencyStr;
            bool isILS = (currencyStr == "ILS");

            Bank::getInstance().lockBank(READER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            acc->lockAccount(WRITER_MODE);

            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }

            int currentBalance = isILS ? acc->balanceILS : acc->balanceUSD;
            if (currentBalance < amount) {
                std::stringstream msg;
                msg << "Error " << id << ": Your transaction failed - account id " << accountId 
                    << " balance is " << acc->balanceILS << " ILS and " << acc->balanceUSD << " USD is lower than " 
                    << amount << " " << currencyStr;
                logError(msg.str());
                acc->unlockAccount(WRITER_MODE);
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            if (isILS) acc->balanceILS -= amount;
            else       acc->balanceUSD -= amount;


            std::stringstream msg;
            msg << id << ": Account " << accountId << " new balance is " << acc->balanceILS
                << " ILS and " << acc->balanceUSD << " USD after " << amount << " " << currencyStr << " was withdrawn";
            logSuccess(msg.str());

            acc->unlockAccount(WRITER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);

            return true;
        }

        // -------------------------------------------------------
        // B: Balance Check
        // Format: B <id> <pass>
        // -------------------------------------------------------
        case 'B': { 
            ss >> accountId >> password;
            
            Bank::getInstance().lockBank(READER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }
            
            // Use Reader Lock for balance check
            acc->lockAccount(READER_MODE);
            
            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(READER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }
            

            

            std::stringstream msg;
            msg << id << ": Account " << accountId << " balance is " << acc->balanceILS << " ILS and " << acc->balanceUSD << " USD";
            logSuccess(msg.str());

            acc->unlockAccount(READER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);
            return true;
        }

        // -------------------------------------------------------
        // T: Transfer
        // Format: T <src> <pass> <dst> <amt> <curr>
        // -------------------------------------------------------
        case 'T': { 
            ss >> accountId >> password >> targetId >> amount >> currencyStr;
            bool isILS = (currencyStr == "ILS");

            Bank::getInstance().lockBank(READER_MODE);

            Account* src = Bank::getInstance().getAccount(accountId);
            Account* dst = Bank::getInstance().getAccount(targetId);

            if (!src) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }
            if (!dst) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(targetId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            // DEADLOCK PREVENTION: Always lock smaller ID first
            Account* first = (src->id < dst->id) ? src : dst;
            Account* second = (src->id < dst->id) ? dst : src;

            first->lockAccount(WRITER_MODE);
            second->lockAccount(WRITER_MODE);

            if (!src->checkPassword(password)) {
                //Error <ATM ID>: Your transaction failed – password for account id <id> is incorrect
                logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                second->unlockAccount(WRITER_MODE);
                first->unlockAccount(WRITER_MODE);
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }


            //Error <ATM ID>: Your transaction failed – balance of account id <id> is lower than <amount> <currency>
            int currentBalance = isILS ? src->balanceILS : src->balanceUSD;
            if (currentBalance < amount) {
                 std::stringstream msg;
                 msg << "Error " << id << ": Your transaction failed - balance of account id " << accountId 
                     << " is lower than " << amount << " " << currencyStr;
                 logError(msg.str());
                 second->unlockAccount(WRITER_MODE);
                 first->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }

            // Execute Transfer
            if (isILS) {
                src->balanceILS -= amount;
                dst->balanceILS += amount;
            } else {
                src->balanceUSD -= amount;
                dst->balanceUSD += amount;
            }

            //<ATM ID>: Transfer <amount> <currency> from account <source account> to account <target account>
            //new account balance is <source balance ILS> ILS and < source balance USD> USD new target account
            //balance is <target balance ILS> ILS and <target balance USD> USD
            std::stringstream msg;
            msg << id << ": Transfer " << amount << " " << currencyStr 
                << " from account " << accountId << " to account " << targetId 
                << " new account balance is " << src->balanceILS << " ILS and " << src->balanceUSD << " USD"
                << " new target account balance is " << dst->balanceILS << " ILS and " << dst->balanceUSD << " USD";
            logSuccess(msg.str());

            second->unlockAccount(WRITER_MODE);
            first->unlockAccount(WRITER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);

            return true;
        }

        // -------------------------------------------------------
        // Q: Close Account
        // Format: Q <id> <pass>
        // -------------------------------------------------------
        case 'Q': {
            ss >> accountId >> password;

            //lock the Bank for writing
            Bank::getInstance().lockBank(WRITER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(WRITER_MODE);
                return false;
            }

            // Verify Password under Read Lock first
            acc->lockAccount(READER_MODE);
            if (!acc->checkPassword(password)) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                acc->unlockAccount(READER_MODE);
                Bank::getInstance().unlockBank(WRITER_MODE);
                return false;
            }
            int savedILS = acc->balanceILS;
            int savedUSD = acc->balanceUSD;
            acc->unlockAccount(READER_MODE);

            // Bank::closeAccount handles the Map Write Lock and deletion
            if (Bank::getInstance().closeAccount(accountId, password)) {
                std::stringstream msg;
                msg << id << ": Account " << accountId << " is now closed. Balance was " << savedILS << " ILS and " << savedUSD << " USD";
                logSuccess(msg.str());
                Bank::getInstance().unlockBank(WRITER_MODE);
                return true;
            } else {
                // If failed here, likely race condition deleted it already
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist"); 
                Bank::getInstance().unlockBank(WRITER_MODE);
                return false;
            }
        }

        // -------------------------------------------------------
        // C: Close ATM
        // Format: C <targetATM_ID>
        // -------------------------------------------------------
        case 'C': {
            ss >> targetId;
            // Bank Printer thread executes this request later.
            Bank::getInstance().requestCloseATM(id, targetId);
            return true; 
        }

        // -------------------------------------------------------
        // S: Sleep (Scheduled Break)
        // Format: S <time_in_msec>
        // -------------------------------------------------------
        case 'S': {
            ss >> sleepTime; // Read time in ms

            std::stringstream msg;
            msg << id << ": Currently on a scheduled break. Service will resume within " << sleepTime << " ms.";
            logSuccess(msg.str());

            // Execute Sleep (Convert ms to us)
            usleep(sleepTime * 1000); 

            return true;
        }

        // -------------------------------------------------------
        // X: Exchange currency
        // Format: X <id> <pass> <src> "to" <dst> <amount>
        // -------------------------------------------------------
        case 'X': { 
            std::string sourceCurrency, toWord, targetCurrency;
            
            // 1. Parse arguments: <id> <pass> <src> "to" <dst> <amount>
            ss >> accountId >> password >> sourceCurrency >> toWord >> targetCurrency >> amount;
            
            // 2. Lock Bank (Reader) - Guarantee account existence
            Bank::getInstance().lockBank(READER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);

            // 3. Existence Check
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            // 4. Lock Account (Writer) - We are modifying balances
            acc->lockAccount(WRITER_MODE);

            // 5. Password Check
            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }

            // 6. Check Funds and Perform Exchange
            bool enoughFunds = false;
            if (sourceCurrency == "ILS") {
                if (acc->balanceILS >= amount) {
                    enoughFunds = true;
                    acc->balanceILS -= amount;
                    acc->balanceUSD += (amount / DOLLAR_TO_ILS_RATE); // integer division (check your specs!)
                }
            } else { // Source is USD
                if (acc->balanceUSD >= amount) {
                    enoughFunds = true;
                    acc->balanceUSD -= amount;
                    acc->balanceILS += (amount * DOLLAR_TO_ILS_RATE);
                }
            }

            // 7. Handle Insufficient Funds
            if (!enoughFunds) {
                // Must print current balances of both currencies according to spec
                 logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + 
                          " balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
                          std::to_string(acc->balanceUSD) + " USD is lower than " + 
                          std::to_string(amount) + " " + sourceCurrency);
                 
                 acc->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }

            // 8. Log Success
            // Format: <ATM ID>: Account <id> new balance is <ILS> ILS and <USD> USD after <amt> <cur> was exchanged
            std::stringstream msg;
            msg << id << ": Account " << accountId << " new balance is " << acc->balanceILS 
                << " ILS and " << acc->balanceUSD << " USD after " << amount << " " 
                << sourceCurrency << " was exchanged";
            logSuccess(msg.str());

            // 9. Unlock
            acc->unlockAccount(WRITER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);

            return true;
        }

        // -------------------------------------------------------
        // I: Investment
        // Format: I <id> <pass> <amount> <currency> <time_in_msec>
        // -------------------------------------------------------
        case 'I': { 
            int timeMillis;
            ss >> accountId >> password >> amount >> currencyStr >> timeMillis;

            // --- LOCKING & CHECKS START ---
            Bank::getInstance().lockBank(READER_MODE);
            Account* acc = Bank::getInstance().getAccount(accountId);

            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            acc->lockAccount(WRITER_MODE);

            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }

            // Check funds and deduct immediately
            bool hasFunds = false;
            if (currencyStr == "ILS") {
                if (acc->balanceILS >= amount) {
                    acc->balanceILS -= amount;
                    hasFunds = true;
                }
            } else { // USD
                if (acc->balanceUSD >= amount) {
                    acc->balanceUSD -= amount;
                    hasFunds = true;
                }
            }

            if (!hasFunds) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + 
                          " balance is " + std::to_string(acc->balanceILS) + " ILS and " + 
                          std::to_string(acc->balanceUSD) + " USD is lower than " + 
                          std::to_string(amount) + " " + currencyStr);
                 
                 acc->unlockAccount(WRITER_MODE);
                 Bank::getInstance().unlockBank(READER_MODE);
                 return false;
            }
            // --- CHECKS END ---

            // --- THREAD CREATION ---
            
            // 1. Pack arguments onto the HEAP
            // We use 'new' because the local variables will die when this case ends.
            InvestmentData* args = new InvestmentData;
            args->atmId = id;
            args->accountId = accountId;
            args->amount = amount;
            args->currency = currencyStr;
            args->timeMillis = timeMillis;

            // 2. Create the thread
            pthread_t investment_thread;
            if (pthread_create(&investment_thread, NULL, ATM::investmentRoutine, (void*)args) != 0) {
                // If thread creation fails, we must refund the money!
                if (currencyStr == "ILS") acc->balanceILS += amount;
                else acc->balanceUSD += amount;
                
                delete args; // Clean up the struct since the thread won't
                logError("Error " + std::to_string(id) + ": System error - failed to create investment thread");
                
                acc->unlockAccount(WRITER_MODE);
                Bank::getInstance().unlockBank(READER_MODE);
                return false;
            }

            // 3. Detach the thread
            // This tells the OS "I don't care about joining this thread, just clean it up when it finishes."
            pthread_detach(investment_thread);

            acc->unlockAccount(WRITER_MODE);
            Bank::getInstance().unlockBank(READER_MODE);

            return true;
        }

        // -------------------------------------------------------
        // R: Rollback
        // Format: R <iterations>
        // -------------------------------------------------------
        case 'R': {
            int iterations;
            ss >> iterations;
            
            // Execute Rollback via Bank
            // Passes 'id' (ATM ID) for the log message
            // Note: The rollback is executed by the Bank's status thread *after* printing the next status.
            Bank::getInstance().requestRollback(id, iterations);

            // Per instructions, the success message is logged inside the Bank::rollback function.
            return true;
        }

        default:
            return false;
    }
}

// --------------------------------------------------------------------------
// Logging Helpers
// --------------------------------------------------------------------------
void ATM::logSuccess(const std::string& msg) {
    LogFile::getInstance().write(msg);
}

void ATM::logError(const std::string& msg) {
    LogFile::getInstance().write(msg);
}

//---------------------------------------------------------------------------
// helper routines for I (investment) command - not 
//--------------------------------------------------------------------------

// Data structure to pass multiple arguments to the investment thread
// Defined in ATM.h as ATM::InvestmentData

// Helper function for the investment thread
void* ATM::investmentRoutine(void* arg) {
    // 1. Unpack and Free Memory
    // We must copy the data to local variables and delete the struct immediately
    InvestmentData* data = (InvestmentData*)arg;
    int atmId = data->atmId;
    int accountId = data->accountId;
    int amount = data->amount;
    std::string currency = data->currency;
    int timeMillis = data->timeMillis;
    delete data; // CRITICAL: Free the heap memory we allocated in the main thread

    (void)atmId; // reserved for potential logging/debug

    // 2. Sleep
    usleep(timeMillis * 1000); // usleep takes microseconds

    // 3. Calculate Return
    // Formula: amount * 1.03^time
    double factor = std::pow(1.03, timeMillis); 
    int finalAmount = std::round(amount * factor);

    // 4. Re-acquire Locks (Standard "Existence Guarantee" pattern)
    Bank::getInstance().lockBank(READER_MODE);
    Account* acc = Bank::getInstance().getAccount(accountId);

    // 5. Update Account
    if (acc) {
        acc->lockAccount(WRITER_MODE);
        
        if (currency == "ILS") {
            acc->balanceILS += finalAmount;
        } else {
            acc->balanceUSD += finalAmount;
        }
        
        // Optional: Log completion here if required by debug/verbose flags
        // logSuccess("Investment completed..."); 

        acc->unlockAccount(WRITER_MODE);
    }
    
    Bank::getInstance().unlockBank(READER_MODE);
    
    return NULL;
}
