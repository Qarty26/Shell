#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

char line[256];
char commands[101][256];
int commandCounter = 0;

void add_to_history(char* line) {
    strcpy(commands[commandCounter++], line);
}

void get_command () {
    if (fgets(line, sizeof(line), stdin) != NULL) {
    
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
    }

    if (strlen(line) > 1) {
        add_to_history(line);
    }
}

void history() {
    for (int i = 0; i < commandCounter; i++) {
        printf("%d %s\n", i + 1, commands[i]);
    }
}

void clear() {
    system("clear");
}

void help() {
    printf("history - show command history\n");
    printf("clear - clear screen\n");
    printf("exit - exit shell\n");
    printf("help - show help\n");
    printf("ls - list directory contents\n");
}

int ls(char** argv) {
    pid_t pid = fork();
    if (pid < 0)
        return errno;
    if (pid == 0){
        strcpy(argv[0], "/usr/bin/ls");

        execve("/usr/bin/ls", argv, NULL);
        perror(NULL);
    }
    else {
        wait(NULL);
    }
}

void execute(char* command) {
    char commandCopy[256];
    strcpy(commandCopy, command);

    char *token = strtok(commandCopy, " ");
    char* actual_command = token;
    char** argv = malloc(101 * sizeof(char*));
    argv[0] = malloc(256 * sizeof(char));
    argv[1] = malloc(256 * sizeof(char));

    token = strtok(NULL, " ");
    int counter = 1;
    while (token != NULL) {
        if (counter != 1)
            argv[counter] = malloc(256 * sizeof(char));
        strcpy(argv[counter++], token);
        token = strtok(NULL, " ");
    }

    argv[counter] = NULL;

    if (strcmp(command, "history") == 0) {
        history();
    } else if (strcmp(command, "clear") == 0) {
        clear();
    } else if (strcmp(command, "exit") == 0) {
        exit(0);
    } else if (strcmp(command, "help") == 0) {
        help();
    } else if (strcmp(actual_command, "ls") == 0) {
        ls(argv);
    } else {
        printf("Invalid command\n");
    }

    for (int i = 0; i < counter; i++) {
        if (argv[i] != NULL)
            free(argv[i]);
    }
    if (argv != NULL)
        free(argv);
}

int main() {
    system("clear");

    while (1) {
        printf("myshell> ");
        get_command();
        execute(line);
    }

    return 0;
}