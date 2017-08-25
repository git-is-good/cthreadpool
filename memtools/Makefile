BUILDFLAGS :=

ifeq ($(DEBUG), 1)
	BUILDFLAGS += -g
endif

TARGETS := test 
HEADERS := memcheck.h hashtable_memcheck.h 
SINGOBJS := hashtable_memcheck.o memcheck.o test.o

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc -c $(BUILDFLAGS) $<

test: $(SINGOBJS)
	gcc -o $@ $^

clean:
	rm -rf *~ $(TARGETS) $(SINGOBJS) 
