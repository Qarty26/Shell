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
char* currentDirectory;

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
    printf("cd - change the working directory\n");
}

int ls(char** command, int size) {
    pid_t pid = fork();
    if (pid < 0)
        return errno;
    if (pid == 0){
        command[size] = NULL;
        execve("/usr/bin/ls", command, NULL);
        perror(NULL);
    }
    else {
        wait(NULL);
    }
}

int cd(char* argv) {
    if(chdir(argv)){
        perror("Eroare");
        return 1;
    }
}

void echo(char** command, int size) {
    for(int i = 1; i < size; ++i){
        printf("%s ", command[i]);
    }
    printf("\n");
}

void execute(char* command) {
    int cnt = 0;
    char** new_command = malloc(101 * sizeof(char*));
    for(int i = 0; i < 101; ++i){
        new_command[i] = malloc(256 * sizeof(char));
    }
    char* copy_command = malloc(256 * sizeof(char));
    strcpy(copy_command, command);
    char* p = strtok(copy_command, " ");
    while(p != NULL){
        strcpy(new_command[cnt++], p);
        p = strtok(NULL, " ");
    }
    char* actual_command = malloc(256 * sizeof(char));
    strcpy(actual_command, new_command[0]);

    if (strcmp(actual_command, "history") == 0 && cnt == 1) {
        history();
    } else if (strcmp(actual_command, "clear") == 0 && cnt == 1) {
        clear();
    } else if (strcmp(actual_command, "exit") == 0 && cnt == 1) {
        exit(0);
    } else if (strcmp(actual_command, "help") == 0 && cnt == 1) {
        help();
    } else if (strcmp(actual_command, "ls") == 0) {
        ls(new_command, cnt);
    } else if (strcmp(actual_command, "cd") == 0) {
        cd(new_command[1]);
    } else if(strcmp(actual_command, "echo") == 0) {
        echo(new_command, cnt);
    } else {
        printf("Invalid command\n");
    }

    for (int i = 0; i < 101; i++) {
        if (new_command[i] != NULL)
            free(new_command[i]);
    }
    if (new_command != NULL)
        free(new_command);
}

int main() {
    system("clear");

    while (1) {
        currentDirectory = malloc(256 * sizeof(char));
        getcwd(currentDirectory, 256);
        printf("%s> ", currentDirectory);
        free(currentDirectory);   
        get_command();
        execute(line);
    }
    return 0;
}