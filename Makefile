CXX = g++
CXXFLAGS = -std=c++17 -o
LIBFLAGS = -lpthread -ljsoncpp -lmysqlclient -lboost_system
DST = server.exe
SRC = main.cpp 
COREDUMP = core.*

RM = rm -rf 

$(DST) : $(SRC)
	$(CXX) $(CXXFLAGS) $(DST) $(SRC) $(LIBFLAGS)

.PHONY:clean
clean:
	$(RM) $(DST) $(COREDUMP)


