CXX := g++
CXXFLAGS := -std=c++11 -g -Wall -Werror -pedantic-errors -DNDEBUG -pthread

# If you add more .cpp files later (e.g., main.cpp), they will be picked up automatically.
SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)

# 1) Required rule: bank
bank: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o bank

# 2) Required: explicit rule for each file (project's current .cpp files)
Account.o: Account.cpp Account.h LockRW.h LogFile.h
	$(CXX) $(CXXFLAGS) -c Account.cpp -o Account.o

ATM.o: ATM.cpp ATM.h Account.h Bank.h LogFile.h
	$(CXX) $(CXXFLAGS) -c ATM.cpp -o ATM.o

Bank.o: Bank.cpp Bank.h Account.h LockRW.h LogFile.h
	$(CXX) $(CXXFLAGS) -c Bank.cpp -o Bank.o

LockRW.o: LockRW.cpp LockRW.h
	$(CXX) $(CXXFLAGS) -c LockRW.cpp -o LockRW.o

LogFile.o: LogFile.cpp LogFile.h
	$(CXX) $(CXXFLAGS) -c LogFile.cpp -o LogFile.o

# Fallback rule (still nice to have; doesn't replace the explicit rules above)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 3) Required rule: clean
.PHONY: clean
clean:
	rm -f *.o bank
