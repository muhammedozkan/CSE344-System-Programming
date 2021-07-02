myFind :  myFind.o
	gcc -o myFind  myFind.o

myFind.o : myFind.c
	gcc -c -ansi -pedantic-errors -std=gnu99 -Wall myFind.c 

clean:
	rm *.o myFind