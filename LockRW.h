#ifndef LOCKRW_H
#define LOCKRW_H

#include <pthread.h>

#define WRITER_MODE true
#define READER_MODE false

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

    void readEnter();
    void readExit();
    void writeEnter();
    void writeExit();
};

#endif