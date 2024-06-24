#execute init_msocket.c
init_msocket: init_msocket.c libmsocket.a
	gcc -o init_msocket init_msocket.c -I. -L. -lmsocket
	./init_msocket

#write makefile to create static library for msocket.c and msocket.h with name libmsocket.a
libmsocket.a: msocket.o
	ar -rcs libmsocket.a msocket.o	

msocket.o: msocket.c msocket.h
	gcc -c msocket.c -o msocket.o
#run user1.c 
user1: libmsocket.a user1.c
	gcc -o user1 user1.c msocket.o -L. -lmsocket
	./user1
#run user2.c
user2: libmsocket.a user2.c
	gcc -o user2 user2.c msocket.o -L. -lmsocket
	./user2

run: init_msocket

	

clean:
	rm -f *.o *.a init_msocket user1 user2 
	

