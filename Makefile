compile:
	gcc -Wall -Wextra src/main.c src/linenoise.c -o nsh -Isrc/libs

clean:
	rm -f nsh
run:
	./nsh

.PHONY: compile clean run
