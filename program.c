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
int running = 1;

void add_to_history(char* line) {
    strcpy(commands[commandCounter++], line);
}

int get_command () {
    if (fgets(line, sizeof(line), stdin) != NULL) {

        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
    }
    
    if (strlen(line) >= 1) {
        add_to_history(line);
        return 1;
    }

    return 0;
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
    printf("echo - show given message\n");
    printf("pwd - show current path\n");
}


void grep(char* pattern) {
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (strstr(buffer, pattern) != NULL) {
            printf("%s", buffer);
        }
    }
}

int ls(char** command, int size, int pipe_fd[]) {
    pid_t pid = fork();
    if (pid < 0)
        return errno;
    if (pid == 0) {
        if (pipe_fd[1] != -1) {
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);
        }

        command[size] = NULL;
        execve("/bin/ls", command, NULL);
        perror(NULL);
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
    }

    return 0;
}

int cd(char* argv) {
    if(chdir(argv)){
        perror("Error");
        return 1;
    }
    return 0;
}

void echo(char** command, int size, int pipe_fd[]) {
    for(int i = 1; i < size; ++i){
        printf("%s ", command[i]);
    }
    printf("\n");

    if (pipe_fd[1] != -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
    }
}

void pwd(int pipe_fd[]) {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));

    printf("%s\n", cwd);

    if (pipe_fd[1] != -1) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
    }
}

void exitt() {
    running = 0;
}

static struct termios old, current;

void initTermios() 
{
  tcgetattr(0, &old); 
  current = old; 
  current.c_lflag &= ~(ICANON | ECHO); 
  tcsetattr(0, TCSANOW, &current); 
}

void resetTermios(void) 
{
  tcsetattr(0, TCSANOW, &old);
}

char getch(void) 
{
  char ch;
  initTermios();
  ch = getchar();
  resetTermios();
  return ch;
}


int history_with_arrows(void)
{
  int first, second;
  int contor = commandCounter;
  int inWhile = 1;

    while(inWhile ==1)
    {

        first = getch();
        if (first == '\x1b') 
        {  
            second = getch();  
            switch (second) 
            {
            case '[':  
                switch (getch()) {  
                case 'A':
                    if(contor > 0)
                        contor--;

                    //takes care of overwriting issue on the line
                    printf("\r");
                    for(int i = 0; i<100; i++)
                        printf(" ");

                    //print the command                                                                               
                    printf("\r%s",commands[contor]);
                    break;  


                case 'B':
                    if(contor < commandCounter - 1)
                        contor++;

                    //takes care of overwriting issue on the line
                    printf("\r");
                    for(int i = 0; i<100; i++)
                        printf(" ");
                    
                    //print the command
                    printf("\r%s",commands[contor]);
                    break;
                }
            default: break;  
            }
        } 
        else 
        {
            inWhile = 0;
            printf("\n");
            break;
        }

    }
}

void execute(char* command) {
    int cnt = 0;
    char** new_command = malloc(101 * sizeof(char*));
    for(int i = 0; i < 101; ++i) {
        new_command[i] = malloc(256 * sizeof(char));
    }

    char* copy_command = malloc(256 * sizeof(char));
    strcpy(copy_command, command);
    char* p = strtok(copy_command, " ");

    while(p != NULL){
        strcpy(new_command[cnt++], p);
        p = strtok(NULL, " ");
    }

    int pipe_fd[2] = { -1, -1 };
    int pipe_index = -1;

    for (int i = 0; i < cnt; i++) {
        if (strcmp(new_command[i], "|") == 0) {
            if (pipe(pipe_fd) == -1) {
                perror("Pipe error");
                return;
            }

            pipe_index = i;
            break;
        }
    }

    if (pipe_index != -1) {
        char* left_command[pipe_index + 1];
        char* right_command[cnt - pipe_index];

        for (int i = 0; i < pipe_index; i++) {
            left_command[i] = new_command[i];
        }
        left_command[pipe_index] = NULL;

        int j = 0;
        for (int i = pipe_index + 1; i < cnt; i++) {
            right_command[j++] = new_command[i];
        }
        right_command[j] = NULL;

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork error");
            return;
        }

        if (pid == 0) { // Child process
            close(pipe_fd[0]); // Close unused read end

            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);

            if (strcmp(left_command[0], "ls") == 0) {
                ls(left_command, pipe_index, pipe_fd);
            } else if (strcmp(left_command[0], "echo") == 0) {
                echo(left_command, pipe_index, pipe_fd);
            } else if (strcmp(left_command[0], "pwd") == 0) {
                pwd(pipe_fd);
            } else if (strcmp(left_command[0], "grep") == 0) {
                grep(left_command[1]);
            } else {
                printf("Invalid command\n");
            }

            exit(EXIT_SUCCESS);
        } else { // Parent process
            close(pipe_fd[1]); // Close unused write end

            pid_t pid2 = fork();

            if (pid2 < 0) {
                perror("Fork error");
                return;
            }

            if (pid2 == 0) { // Child process
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);

                if (strcmp(right_command[0], "ls") == 0) {
                    ls(right_command, cnt - pipe_index - 1, pipe_fd);
                } else if (strcmp(right_command[0], "echo") == 0) {
                    echo(right_command, cnt - pipe_index - 1, pipe_fd);
                } else if (strcmp(right_command[0], "pwd") == 0) {
                    pwd(pipe_fd);
                } else if (strcmp(right_command[0], "grep") == 0) {
                    grep(right_command[1]);
                } else {
                    printf("Invalid command\n");
                }

                exit(EXIT_SUCCESS);
            } else { // Parent process
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                waitpid(pid, NULL, 0);
                waitpid(pid2, NULL, 0);
            }
        }
    } else {
        char* actual_command = new_command[0];

        if (strcmp(actual_command, "history") == 0 && cnt == 1) {
            history();
        } else if (strcmp(actual_command, "clear") == 0 && cnt == 1) {
            clear();
        } else if (strcmp(actual_command, "exit") == 0) {
            exitt();
        } else if (strcmp(actual_command, "help") == 0 && cnt == 1) {
            help();
        } else if (strcmp(actual_command, "ls") == 0) {
            ls(new_command, cnt, pipe_fd);
        } else if (strcmp(actual_command, "cd") == 0) {
            cd(new_command[1]);
        } else if(strcmp(actual_command, "echo") == 0) {
            echo(new_command, cnt, pipe_fd);
        } else if(strcmp(actual_command, "pwd") == 0) {
            pwd(pipe_fd);
        } else if(strcmp(actual_command, "grep") == 0) {
            grep(new_command[1]);
        } else if(strcmp(actual_command, "historywa") == 0) {
            history_with_arrows();
        } else {
            printf("Invalid command\n");
        }
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
    currentDirectory = malloc(256 * sizeof(char));

    while (running) {
        getcwd(currentDirectory, 256);
        printf("%s> ", currentDirectory);
        if(get_command())
            execute(line);
    }
    free(currentDirectory);
    return 0;
}