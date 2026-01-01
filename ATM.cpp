#include "ATM.h"
#include "Bank.h"
#include "Account.h"
#include "LogFile.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>  // For usleep
#include <algorithm> // For std::min, std::max

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

            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                return false;
            }

            // Lock for writing (modifying balance)
            acc->lockAccount(WRITER_MODE);

            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
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

            return true;
        }

        // -------------------------------------------------------
        // W: Withdraw
        // Format: W <id> <pass> <amount> <currency>
        // -------------------------------------------------------
        case 'W': { 
            ss >> accountId >> password >> amount >> currencyStr;
            bool isILS = (currencyStr == "ILS");

            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                return false;
            }

            acc->lockAccount(WRITER_MODE);

            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(WRITER_MODE);
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
                return false;
            }

            if (isILS) acc->balanceILS -= amount;
            else       acc->balanceUSD -= amount;

            int balILS = acc->balanceILS;
            int balUSD = acc->balanceUSD;

            acc->unlockAccount(WRITER_MODE);

            std::stringstream msg;
            msg << id << ": Account " << accountId << " new balance is " << balILS 
                << " ILS and " << balUSD << " USD after " << amount << " " << currencyStr << " was withdrawn";
            logSuccess(msg.str());
            return true;
        }

        // -------------------------------------------------------
        // B: Balance Check
        // Format: B <id> <pass>
        // -------------------------------------------------------
        case 'B': { 
            ss >> accountId >> password;
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                 return false;
            }
            
            // Use Reader Lock for balance check
            acc->lockAccount(READER_MODE);
            
            if (!acc->checkPassword(password)) {
                 logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                 acc->unlockAccount(READER_MODE);
                 return false;
            }
            
            int balILS = acc->balanceILS;
            int balUSD = acc->balanceUSD;
            
            acc->unlockAccount(READER_MODE);

            std::stringstream msg;
            msg << id << ": Account " << accountId << " balance is " << balILS << " ILS and " << balUSD << " USD";
            logSuccess(msg.str());
            return true;
        }

        // -------------------------------------------------------
        // T: Transfer
        // Format: T <src> <pass> <dst> <amt> <curr>
        // -------------------------------------------------------
        case 'T': { 
            ss >> accountId >> password >> targetId >> amount >> currencyStr;
            bool isILS = (currencyStr == "ILS");

            Account* src = Bank::getInstance().getAccount(accountId);
            Account* dst = Bank::getInstance().getAccount(targetId);

            if (!src) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                return false;
            }
            if (!dst) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(targetId) + " does not exist");
                return false;
            }

            // DEADLOCK PREVENTION: Always lock smaller ID first
            Account* first = (src->id < dst->id) ? src : dst;
            Account* second = (src->id < dst->id) ? dst : src;

            first->lockAccount(WRITER_MODE);
            second->lockAccount(WRITER_MODE);

            if (!src->checkPassword(password)) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                second->unlockAccount(WRITER_MODE);
                first->unlockAccount(WRITER_MODE);
                return false;
            }

            int currentBalance = isILS ? src->balanceILS : src->balanceUSD;
            if (currentBalance < amount) {
                 std::stringstream msg;
                 msg << "Error " << id << ": Your transaction failed - account id " << accountId 
                     << " balance is " << src->balanceILS << " ILS and " << src->balanceUSD 
                     << " USD is lower than " << amount << " " << currencyStr;
                 logError(msg.str());
                 second->unlockAccount(WRITER_MODE);
                 first->unlockAccount(WRITER_MODE);
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

            // Capture state for logging
            int sBalILS = src->balanceILS;
            int sBalUSD = src->balanceUSD;
            int dBalILS = dst->balanceILS;
            int dBalUSD = dst->balanceUSD;

            second->unlockAccount(WRITER_MODE);
            first->unlockAccount(WRITER_MODE);

            std::stringstream msg;
            msg << id << ": Transfer " << amount << " " << currencyStr 
                << " from account " << accountId << " to account " << targetId 
                << " new account balance is " << sBalILS << " ILS and " << sBalUSD << " USD"
                << " new target account balance is " << dBalILS << " ILS and " << dBalUSD << " USD";
            logSuccess(msg.str());
            return true;
        }

        // -------------------------------------------------------
        // Q: Close Account
        // Format: Q <id> <pass>
        // -------------------------------------------------------
        case 'Q': {
            ss >> accountId >> password;
            Account* acc = Bank::getInstance().getAccount(accountId);
            if (!acc) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist");
                return false;
            }

            // Verify Password under Read Lock first
            acc->lockAccount(READER_MODE);
            if (!acc->checkPassword(password)) {
                logError("Error " + std::to_string(id) + ": Your transaction failed - password for account id " + std::to_string(accountId) + " is incorrect");
                acc->unlockAccount(READER_MODE);
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
                return true;
            } else {
                // If failed here, likely race condition deleted it already
                logError("Error " + std::to_string(id) + ": Your transaction failed - account id " + std::to_string(accountId) + " does not exist"); 
                return false;
            }
        }

        // -------------------------------------------------------
        // C: Close ATM
        // Format: C <targetATM_ID>
        // -------------------------------------------------------
        case 'C': {
            ss >> targetId;
            [cite_start]// "ATM will ask the bank to close the ATM" [cite: 144]
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