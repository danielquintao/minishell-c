all: minishell
minishell: minishell.c
	gcc minishell.c -o minishell
	./minishell
clean:
	rm -rf *.o minishell