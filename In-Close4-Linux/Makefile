CC=icpc
CFLAGS=-c -O2
LDFLAGS=-fopenmp -O2 
SOURCES=In-Close4.cpp  hr_time.cpp crossplatform.cpp 
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=Inclose

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
clean: 
	rm -f $(OBJECTS) $(TARGET) *.optrpt

