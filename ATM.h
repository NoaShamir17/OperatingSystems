#ifndef ATM_H
#define ATM_H

#include <string>
#include <pthread.h>
#include <vector>

class ATM {
private:
    int id;
    std::string inputFilePath;
    bool active;       // The flag to control ATM lifecycle
    pthread_t thread;  // The thread running this ATM

    // Helper to parse and execute a single line
    // Returns true if successful, false if failed (for Persistent logic)
    bool processCommand(const std::string& line);

    // Specific command helpers
    void logSuccess(const std::string& msg);
    void logError(const std::string& msg);

public:
    ATM(int id, const std::string& filePath);
    ~ATM();

    // The function passed to pthread_create
    static void* startRoutine(void* arg);

    // The main loop
    void run();

    // Called by the Bank to signal this ATM to stop
    void close(); 
    
    // Check if ATM is still running (useful for Bank cleanup)
    bool isActive() const;

    // Getters
    int getId() const { return id; }
    pthread_t getThread() const { return thread; }
};

#endif