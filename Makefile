BUILDFLAGS :=

ifeq ($(DEBUG), 1)
	BUILDFLAGS += -g
endif

TARGETS := test 
HEADERS := threadpool.h
SINGOBJS := threadpool.o test.o

all: $(TARGETS)

%.o: %.c $(HEADERS)
	gcc -c $(BUILDFLAGS) $<

test: $(SINGOBJS)
	gcc -o $@ $^

clean:
	rm -rf *~ $(TARGETS) $(SINGOBJS) 
