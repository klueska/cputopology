CFILES = cputopology.c acpi.c arch.c
EXEC = cputopology
LIBS = -lpthread

all: $(CFILES)
	gcc -std=gnu99 -o $(EXEC) $(CFILES) $(LIBS)

clean:
	rm -rf $(EXEC)
