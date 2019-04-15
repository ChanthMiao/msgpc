
msgpc: msgpc.c predir
	gcc -o bin/msgpc msgpc.c

.PHONY: clean predir

clean:
	@rm -rf bin
predir:
	@mkdir -p bin