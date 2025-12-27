#include "LockRW.h"

LockRW::LockRW() : readers(0), writersWaiting(0), writerActive(false) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&readCond, NULL);
    pthread_cond_init(&writeCond, NULL);
}

LockRW::~LockRW() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&readCond);
    pthread_cond_destroy(&writeCond);
}

void LockRW::readEnter() {
    pthread_mutex_lock(&mutex);
    // Wait if there is an active writer OR writers are waiting (Writer Preference)
    while (writersWaiting > 0 || writerActive) {
        pthread_cond_wait(&readCond, &mutex);
    }
    readers++;
    pthread_mutex_unlock(&mutex);
}

void LockRW::readExit() {
    pthread_mutex_lock(&mutex);
    readers--;
    if (readers == 0) {
        // Signal writers if no more readers
        pthread_cond_signal(&writeCond);
    }
    pthread_mutex_unlock(&mutex);
}

void LockRW::writeEnter() {
    pthread_mutex_lock(&mutex);
    writersWaiting++;
    while (readers > 0 || writerActive) {
        pthread_cond_wait(&writeCond, &mutex);
    }
    writersWaiting--;
    writerActive = true;
    pthread_mutex_unlock(&mutex);
}

void LockRW::writeExit() {
    pthread_mutex_lock(&mutex);
    writerActive = false;
    // Broadcast to readers or signal one writer. 
    // Generally, let writers fight or wake all readers.
    pthread_cond_broadcast(&writeCond); 
    pthread_cond_broadcast(&readCond);
    pthread_mutex_unlock(&mutex);
}