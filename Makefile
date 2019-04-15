
MODE = release
ifeq ($(MODE),debug)
        CFLAG = -g
else
        CFLAG = -O3
endif

msgpc: msgpc.c predir
	gcc -o bin/msgpc $(CFLAG) msgpc.c

.PHONY: clean predir

clean:
	@rm -rf bin
predir:
	@mkdir -p bin