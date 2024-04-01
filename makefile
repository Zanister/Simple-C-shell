LEX = lex
CC = gcc
CFLAGS =

all: myshell

lex.yy.c: lex.l
	$(LEX) lex.l

myshell: lex.yy.c myshell.c
	$(CC) -o myshell lex.yy.c myshell.c -lfl

clean:
	rm -f lex.yy.c myshell
