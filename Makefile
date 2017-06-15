CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=104494120
CLASSES=udpsocket.cpp udpmux.cpp cfp.cpp util.cpp timer.cpp eventloop.cpp

all: server client

server: $(CLASSES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

client: $(CLASSES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz \
	--exclude=./.vagrant \
	--exclude=sample-* \
	--exclude=*.txt \
   	. && \
	mv /tmp/$(USERID).tar.gz .
