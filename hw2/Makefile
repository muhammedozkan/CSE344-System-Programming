processM :  processM.o
	gcc -o processM  processM.o -lm

processM.o : processM.c
	gcc -c -ansi -pedantic-errors -std=gnu99 -Wall processM.c 
	
clean:
	rm *.o processM