#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#define BUFFER_SIZE 1024

char line[256];
char commands[1000][256];
int commandCounter = 0;
char* currentDirectory;
int running = 1;
struct termios old;

void add_to_history(char* line) {
    if (commandCounter <= 999) {
        strcpy(commands[commandCounter++], line);
    } else {
        for (int i = 0; i < 999; i++) {
            strcpy(commands[i], commands[i + 1]);
        }
        strcpy(commands[999], line);
    }
}

void load_history() {
    FILE* file = fopen("history.txt", "r");
    if (file == NULL) {
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[strlen(buffer) - 1] = '\0';
        add_to_history(buffer);
    }

    fclose(file);
}

void upload_history() {
    FILE* file = fopen("history.txt", "w");
    if (file == NULL) {
        return;
    }

    for (int i = 0; i < commandCounter; i++) {
        if (strcmp(commands[i], "exit") != 0)
            fprintf(file, "%s\n", commands[i]);
    }

    fclose(file);
}

void initTermios(int echo) {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &old); // save old settings
    newt = old; // new settings
    newt.c_lflag &= ~ICANON; // disable buffered io
    if (echo) {
        newt.c_lflag |= ECHO; // set echo mode
    } else {
        newt.c_lflag &= ~ECHO; // set no echo mode
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // use these new terminal i/o settings now
}

void resetTermios(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

char getch_(int echo) {
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

char getch(void) {
    return getch_(0);
}

int get_command() {
    int index = commandCounter - 1;
    int cursorPosition = 0;
    char input;
    char tempCommand[256] = "";
    int commandLength = 0;
    char last_UP_DOWN_arrow;

    initTermios(0);

    while (1) {
        input = getch();

        if (input == 27) { // arrow keys
            getch(); // skip [ [[A
            switch(getch()) { // the real value
                case 'A': // up
                    if (index >= 0) {
                        if (last_UP_DOWN_arrow == 'B')
                            index--;
                        last_UP_DOWN_arrow = 'A';
                        strcpy(tempCommand, commands[index--]);
                        cursorPosition = strlen(tempCommand);
                        commandLength = cursorPosition;
                        printf("\33[2K\r"); // Clear the line
                        printf("%s> %s", currentDirectory, tempCommand);
                    }
                    break;
                case 'B': // down
                    if (index < commandCounter - 1) {
                        if (last_UP_DOWN_arrow == 'A')
                            index++;
                        last_UP_DOWN_arrow = 'B';
                        strcpy(tempCommand, commands[++index]);
                        cursorPosition = strlen(tempCommand);
                        commandLength = cursorPosition;
                        printf("\33[2K\r"); // Clear the line
                        printf("%s> %s", currentDirectory, tempCommand);
                    }
                    else {
                        strcpy(tempCommand, "");
                        cursorPosition = 0;
                        commandLength = 0;
                        printf("\33[2K\r");
                        printf("%s> %s", currentDirectory, tempCommand);
                    }
                    break;
                
                case 'C': // right arrow
                    if (cursorPosition < commandLength) {
                        cursorPosition++;
                        printf("\033[C"); // Move cursor right
                    }
                    break;
                case 'D': // left arrow
                    if (cursorPosition > 0) {
                        cursorPosition--;
                        printf("\033[D"); // Move cursor left
                    }
                    break;
            }
        } else if (input == 10) { // enter key
            printf("\n");
            execute(tempCommand);
            break;
        } else if (input == 127) { // backspace
            if (cursorPosition > 0) {
                memmove(&tempCommand[cursorPosition - 1], &tempCommand[cursorPosition], strlen(tempCommand) - cursorPosition + 1);
                cursorPosition--;
                commandLength--;
                printf("\033[2K\r%s> %s", currentDirectory, tempCommand);
                printf("\033[%dG", cursorPosition + strlen(currentDirectory) + 3); // Move cursor to correct position
            }
        } else {
            // Insert character at cursor position
            memmove(&tempCommand[cursorPosition + 1], &tempCommand[cursorPosition], strlen(tempCommand) - cursorPosition);
            tempCommand[cursorPosition] = input;
            cursorPosition++;
            commandLength++;

            printf("\033[2K\r%s> %s", currentDirectory, tempCommand);
            printf("\033[%dG", cursorPosition + strlen(currentDirectory) + 3);
        }
    }

    resetTermios();

    if (commandLength > 0) {
        tempCommand[commandLength] = '\0';
        add_to_history(tempCommand);
        strcpy(line, tempCommand);
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
    printf("historywa - scroll with arrows through history");
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

// static struct termios old, current;

// void initTermios() 
// {
//   tcgetattr(0, &old); 
//   current = old; 
//   current.c_lflag &= ~(ICANON | ECHO); 
//   tcsetattr(0, TCSANOW, &current); 
// }

// void resetTermios(void) 
// {
//   tcsetattr(0, TCSANOW, &old);
// }


int history_with_arrows(void)
{
  int first, second;
  int contor = commandCounter;
  int inWhile = 1;
  char tempCommand[256];
  strcpy(tempCommand, "");


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

                    //print and save the command
                    strcpy(tempCommand, commands[contor]);                                                                               
                    printf("\r%s",commands[contor]);
                    break;  


                case 'B':
                    if(contor < commandCounter - 1)
                        contor++;

                    //takes care of overwriting issue on the line
                    printf("\r");
                    for(int i = 0; i<100; i++)
                        printf(" ");
                    
                    //print and save the command
                    strcpy(tempCommand, commands[contor]);
                    printf("\r%s",commands[contor]);
                    break;
                }
            default: break;  
            }
        }
        else if(first == 127)
        {
            if(strcmp(tempCommand,"") != 0)
            {
                printf("\r");
                for(int i = 0; i<100; i++)
                    printf(" ");
                int len = strlen(tempCommand);
                strcpy(tempCommand + len - 1, tempCommand + len);
                printf("\r%s",tempCommand); 
            }
        }
        else if(first == 10)
        {
            printf("\n");
            execute(tempCommand);
            inWhile = 0;
            break;
        }
        else 
        {
            char charString[2] = {first, '\0'};
            strcat(tempCommand, charString);
            printf("\r");
            for(int i = 0; i<100; i++)
                printf(" ");
            printf("\r%s",tempCommand); 
        }

    }
}

int check_exist(const char *path) {
    if (access(path, F_OK) != -1) {
        return 1;
    } else {
        return 0;
    }
}

int check_file(const char *path) {

    if (access(path, F_OK) != -1) {

        if (access(path, R_OK | X_OK | W_OK) != -1) {
            return 0; 
        } else {
            return 1; 
        }
    } else {
        return 0; 
    }
}

int check_directory(const char *path) {

    if (access(path, F_OK) != -1) {

        if (access(path, R_OK | X_OK | W_OK) != -1) {
            return 1; 
        } else {
            return 0; 
        }
    } else {
        return 0; 
    }
}

// if (!) test <conditie> (&& sau || (!) test <alta_conditie>) ; then <do_something>
void ex_if(char** command, int size) {
    int condition_met = 1; // Initially true for first condition
    int i = 1; // Start from index 1 to skip the 'if' command itself

    while (i < size) {
        char* part = command[i];

        if (strcmp(part, "test") == 0) {
            // Implement your test conditions here
            // Example: if test -f somefile
            if (i + 2 < size && strcmp(command[i + 1], "-f") == 0) {
                condition_met = check_file(command[i + 2]);
                i += 3; // Skip to next part after test condition
            }
            if (i + 2 < size && strcmp(command[i + 1], "-d") == 0) {
                condition_met = check_directory(command[i + 2]);
                i += 3; // Skip to next part after test condition
            }
            if (i + 2 < size && strcmp(command[i + 1], "-e") == 0) {
                condition_met = check_exist(command[i + 2]);
                i += 3; // Skip to next part after test condition
            }
            if (i + 2 < size && strcmp(command[i + 1], "-s") == 0) {
                struct stat stat_record;
                if (stat(command[i + 2], &stat_record)) {
                    perror("Stat error");
                    return;
                }
                if (stat_record.st_size <= 1) {
                    condition_met = 1;
                }
                else {
                    condition_met = 0;
                }
                i += 3;
            }
            if (i + 2 < size && strcmp(command[i + 1], "-r") == 0) {

                if (access(command[i+2], R_OK) == 0) {
                    condition_met = 1;
                } else {
                    condition_met = 0;
                    perror("Error");
                }
                i+=3;
            }
            if (i + 2 < size && strcmp(command[i + 1], "-w") == 0) {

                if (access(command[i+2], W_OK) == 0) {
                    condition_met = 1;
                } else {
                    condition_met = 0;
                    perror("Error");
                }
                i+=3;
            }
            if (i + 2 < size && strcmp(command[i + 1], "-x") == 0) {

                if (access(command[i+2], X_OK) == 0) {
                    condition_met = 1;
                } else {
                    condition_met = 0;
                    perror("Error");
                }
                i+=3;
            }
        } else if (strcmp(part, "&&") == 0) {
            if (!condition_met) {
                break; // Exit if previous condition failed
            }
            i++;
        } else if (strcmp(part, "||") == 0) {
            if (condition_met) {
                while(strcmp(command[i], "then")){
                    i++;
                }
                i++;
                break;
            }
            i++;
        } else if(condition_met){
            i++;
            break;
        } else{
            break;
        }
    }
    if (condition_met) {
        // Execute the command if condition is met
        char new_command[256] = "";
        int new_command_size = 0;
        for (int j = i; j < size && strcmp(command[j], "else") != 0; ++j) {
            strcat(new_command, command[j]);
            if(j != size - 1){
                strcat(new_command, " ");
            }
        }
        execute(new_command);
    }
    else {
        while (strcmp(command[i], "else") != 0 && i < size) {
            i++;
        }
        if (i == size) {
            return;
        }
        char new_command[256] = "";
        for (int j = i + 1; j < size; ++j) {
            strcat(new_command, command[j]);
            if(j != size - 1){
                strcat(new_command, " ");
            }
        }
        execute(new_command);
    }
}
// if test -f /home/andrei/Desktop/file.txt echo Ceva

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

    int status;
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
        } else if(strcmp(actual_command, "if") == 0){
            ex_if(new_command, cnt);
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

    load_history();
    system("clear");
    currentDirectory = malloc(256 * sizeof(char));

    while (running) {
        getcwd(currentDirectory, 256);
        printf("%s> ", currentDirectory);
        get_command();
    }
    free(currentDirectory);
    upload_history();
    return 0;
}