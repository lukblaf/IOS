NAME=proj2

CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LDFLAGS=-lrt -pthread

all: $(NAME).o
	gcc $(CFLAGS) $< -o $(NAME) $(LDFLAGS)

$(NAME).o: $(NAME).c
	gcc $(CFLAGS) -c $< -o $(NAME).o 

zip:
	zip $(NAME).zip $(NAME).c Makefile

clean: 
	rm -f *.o *.out
