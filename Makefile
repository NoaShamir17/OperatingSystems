CXX := g++
CXXFLAGS := -std=c++11 -g -Wall -Werror -pedantic-errors -DNDEBUG -pthread

OBJS := Account.o ATM.o Bank.o LockRW.o LogFile.o main.o

bank: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o bank

Account.o: Account.cpp Account.h LockRW.h LogFile.h
	$(CXX) $(CXXFLAGS) -c Account.cpp -o Account.o

ATM.o: ATM.cpp ATM.h Bank.h Account.h LogFile.h
	$(CXX) $(CXXFLAGS) -c ATM.cpp -o ATM.o

Bank.o: Bank.cpp Bank.h ATM.h Account.h LockRW.h LogFile.h
	$(CXX) $(CXXFLAGS) -c Bank.cpp -o Bank.o

LockRW.o: LockRW.cpp LockRW.h
	$(CXX) $(CXXFLAGS) -c LockRW.cpp -o LockRW.o

LogFile.o: LogFile.cpp LogFile.h
	$(CXX) $(CXXFLAGS) -c LogFile.cpp -o LogFile.o

main.o: main.cpp Bank.h ATM.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

.PHONY: clean
clean:
	rm -f *.o bank
