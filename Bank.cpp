#include "Bank.h"

//=====================Noa Added These Methods=========================
//lock Management
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

//=====================End of Noa Added These Methods=========================

