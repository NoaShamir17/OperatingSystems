#include "LogFile.h"
#include <iostream>

LogFile::LogFile() {
    pthread_mutex_init(&writeMutex, NULL);
    // Use 'trunc' to clear the file content when opening
    logStream.open("log.txt", std::ofstream::out | std::ofstream::trunc); 
}

LogFile::~LogFile() {
    if (logStream.is_open()) {
        logStream.close();
    }
    pthread_mutex_destroy(&writeMutex);
}

LogFile& LogFile::getInstance() {
    static LogFile instance;
    return instance;
}

void LogFile::write(const std::string& message) {
    pthread_mutex_lock(&writeMutex);
    if (logStream.is_open()) {
        logStream << message << std::endl;
    }
    pthread_mutex_unlock(&writeMutex);
}