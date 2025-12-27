#ifndef LOCKRW_H
#define LOCKRW_H

#include <pthread.h>

// A generic Readers-Writers lock using pthreads [cite: 22, 262]
class LockRW {
private:
    pthread_mutex_t mutex;
    pthread_cond_t readCond;
    pthread_cond_t writeCond;
    
    int readers;
    int writersWaiting;
    bool writerActive;

public:
    LockRW();
    ~LockRW();

    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();
};

#endif