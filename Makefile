BUILDFLAGS :=

ifeq ($(DEBUG), 1)
	BUILDFLAGS += -g -DDEBUG
endif

# $(MEMTOOLSDIR)
MEMTOOLSDIR   := memtools
MEMTOOLSHEADS := $(MEMTOOLSDIR)/memcheck.h $(MEMTOOLSDIR)/hashtable_memcheck.h $(MEMTOOLSDIR)/fatalerror.h
MEMTOOLSOBJS  := $(MEMTOOLSDIR)/memcheck.o $(MEMTOOLSDIR)/hashtable_memcheck.o

$(MEMTOOLSDIR)/%.o: $(MEMTOOLSDIR)/%.c $(MEMTOOLSHEADS)
	gcc -o $@ -c $(BUILDFLAGS) $<
# end $(MEMTOOLSDIR)

TARGETS := test 
HEADERS := threadpool.h fatalerror.h 
OBJS := threadpool.o test.o	

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc -o $@ -c $(BUILDFLAGS) $<

test: $(OBJS) $(MEMTOOLSOBJS)
	gcc -o $@ $^

clean:
	rm -rf *~ $(TARGETS) $(OBJS) $(MEMTOOLSOBJS)



