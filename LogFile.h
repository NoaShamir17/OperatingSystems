#ifndef LOGFILE_H
#define LOGFILE_H

#include <fstream>
#include <string>
#include <pthread.h>

class LogFile {
private:
    std::ofstream logStream;
    pthread_mutex_t writeMutex;
    
    // Private constructor for Singleton
    LogFile(); 

public:
    // Singleton access
    static LogFile& getInstance();
    
    ~LogFile();
    void write(const std::string& message);
};

#endif