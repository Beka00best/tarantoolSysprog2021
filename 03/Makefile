F = -c -Wreturn-type

all: test.o userfs.o
	gcc test.o userfs.o

test.o: test.c
	gcc $F test.c -o test.o

userfs.o: userfs.c
	gcc $F userfs.c -o userfs.o

clean:
	rm *.o a.out

clear:
	rm *.o a.out