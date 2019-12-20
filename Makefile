# Target library
lib := libuthread.a
libObj := sem.o tps.o thread.o queue.o
objs := sem.o tps.o

CC := gcc
CFLAGS := -Wall -Werror

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

## TODO: Phase 1 and Phase 2

$(lib): $(libObj)
	ar rcs $(lib) $(libObj)

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CLFAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps) $(libObj)
