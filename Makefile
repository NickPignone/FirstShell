CC=gcc
CFLAGS=-g -O0 -Wall -std=c99
#add headers to DEPS, otherwise make wont detect changes and recompile
#when headers change
DEPS =

# Shell
SH_SRC = A5CS224Fall2015.c makeargv.c error.c
SH_OBJ = A5CS224Fall2015.o makeargv.o error.o
SH_EXEC = A5CS224Fall2015

#.PHONY declaration just says that 'all' isn't a file name.
.PHONY: all
all: $(SH_EXEC)

#compile all .o into executable
#%.o == all .o files
#$@ = left side of :
#$< = right side of :
%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

$(SH_EXEC): $(SH_OBJ) $(DEPS)
	$(CC) -o $@ $^ $(CFLAGS)

#https://www.gnu.org/software/make/manual/html_node/Cleanup.html#Cleanup
.PHONY: clean
clean:
	find . -maxdepth 1 -type f -perm +111 ! -name "*.sh" -exec rm {} +
	find . -maxdepth 1 -type f -name "*.o" -print -exec rm {} +

#MakeFile found at https://www.gnu.org/software/make/