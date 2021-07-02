player :  player.o
	gcc -o player  player.o -lrt -pthread 

player.o : player.c
	gcc -c -ansi -pedantic-errors -std=gnu99 -Wall player.c 
	
clean: cls
	-rm *.o player