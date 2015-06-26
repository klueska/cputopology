CFILES = main.c topology.c acpi.c arch.c schedule.c 
EXEC = cputopology
LIBS = -lpthread

all: $(CFILES) 
	gcc -g -std=gnu99 -o $(EXEC) $(CFILES) $(LIBS) 

clean:
	rm -rf $(EXEC)
