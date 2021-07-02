program :  client.o server.o
	gcc -o client  client.o -lrt -pthread 
	gcc -o server  server.o -lrt -pthread 

client.o : client.c
	gcc -c -ansi -pedantic-errors -std=gnu99 -Wall client.c 
	
server.o : server.c
	gcc -c -ansi -pedantic-errors -std=gnu99 -Wall server.c 	

clean:
	-rm *.o client server