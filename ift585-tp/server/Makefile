# =============================================================
#  Makefile – Serveur IFT585-TP4
# =============================================================
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
TARGET   = server_ift585

SRCS = main.cpp \
       ServerCore.cpp \
       AuthUDPHandler.cpp \
       RESTServer.cpp \
       PersistenceManager.cpp \
       ../common/sha256.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# Compilation manuelle (alternative sans make)
manual:
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)
