#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <errno.h>

#define TIME_QUANTUM 2
#define MAX_PROCESSES 5
#define MEMORY_SIZE 1024

typedef enum {
    NEW,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

typedef struct {
    int pid;
    char name[256];
    ProcessState state;
    int burst_time;
} Process;

typedef struct {
    int size;
    int is_free;
    int start_address;
} MemoryBlock;

MemoryBlock memory[MEMORY_SIZE];
int memory_blocks = 0;

void list_files(const char *path);
void change_directory(const char *path);
void create_file(const char *filename);
void remove_file(const char *filename);
void *execute_process(void *arg);
void schedule_processes(Process processes[], int count);
void authenticate_user();
void allocate_memory(int size);
void deallocate_memory(int block_number);
void display_memory();
void error(const char *msg);
void flush_input();
void disable_echo();
void enable_echo();

int main() {
    char command[256];
    char path[256];
    char filename[256];
    char username[256];
    char password[256];
    int authenticated = 0;
    Process processes[MAX_PROCESSES] = {
        {1, "Process1", NEW, 5}, {2, "Process2", NEW, 3},
        {3, "Process3", NEW, 4}, {4, "Process4", NEW, 2},
        {5, "Process5", NEW, 1}
    };

    authenticate_user(&authenticated);
    if (!authenticated) {
        printf("Authentication failed. Exiting program.\n");
        return 1;
    }

    while (1) {
        printf("\nEnter command (ls, cd, touch, rm, run, alloc, free, mem, exit): ");
        if (scanf("%255s", command) != 1) {
            error("Invalid input");
            flush_input();
            continue;
        }

        if (strcmp(command, "ls") == 0) {
            getcwd(path, sizeof(path));
            list_files(path);
        } else if (strcmp(command, "cd") == 0) {
            printf("Enter path: ");
            if (scanf("%255s", path) != 1) {
                error("Invalid input");
                flush_input();
                continue;
            }
            change_directory(path);
        } else if (strcmp(command, "touch") == 0) {
            printf("Enter filename to create: ");
            if (scanf("%255s", filename) != 1) {
                error("Invalid input");
                flush_input();
                continue;
            }
            create_file(filename);
        } else if (strcmp(command, "rm") == 0) {
            printf("Enter filename to remove: ");
            if (scanf("%255s", filename) != 1) {
                error("Invalid input");
                flush_input();
                continue;
            }
            remove_file(filename);
        } else if (strcmp(command, "run") == 0) {
            schedule_processes(processes, MAX_PROCESSES);
        } else if (strcmp(command, "alloc") == 0) {
            int size;
            printf("Enter memory size to allocate: ");
            if (scanf("%d", &size) != 1) {
                error("Invalid input");
                flush_input();
                continue;
            }
            allocate_memory(size);
        } else if (strcmp(command, "free") == 0) {
            int block_number;
            printf("Enter memory block number to free: ");
            if (scanf("%d", &block_number) != 1) {
                error("Invalid input");
                flush_input();
                continue;
            }
            deallocate_memory(block_number);
        } else if (strcmp(command, "mem") == 0) {
            display_memory();
        } else if (strcmp(command, "exit") == 0) {
            printf("Exiting program.\n");
            break;
        } else {
            printf("Invalid command\n");
        }
    }

    return 0;
}

/* Function Definitions */

void list_files(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        error("opendir");
        return;
    }

    while ((entry = readdir(dp))) {
        printf("%s\n", entry->d_name);
    }

    closedir(dp);
}

void change_directory(const char *path) {
    if (chdir(path) != 0) {
        error("chdir");
    }
}

void create_file(const char *filename) {
    int fd = creat(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        error("creat");
    } else {
        printf("File '%s' created successfully.\n", filename);
        close(fd);
    }
}

void remove_file(const char *filename) {
    if (remove(filename) == 0) {
        printf("File '%s' removed successfully.\n", filename);
    } else {
        error("remove");
    }
}

void *execute_process(void *arg) {
    Process *process = (Process *)arg;

    // Simulate process states
    process->state = RUNNING;
    printf("Process %d (%s) is running.\n", process->pid, process->name);

    sleep(process->burst_time); // Simulate execution time

    process->state = TERMINATED;
    printf("Process %d (%s) has terminated.\n", process->pid, process->name);

    return NULL;
}

void schedule_processes(Process processes[], int count) {
    printf("Scheduling processes using Round Robin algorithm.\n");
    pthread_t threads[MAX_PROCESSES];

    for (int i = 0; i < count; i++) {
        if (processes[i].state == NEW || processes[i].state == READY) {
            processes[i].state = READY;
            pthread_create(&threads[i], NULL, execute_process, &processes[i]);
            pthread_join(threads[i], NULL);
        }
    }
}

void authenticate_user(int *authenticated) {
    char username[256];
    char password[256];
    char correct_password[256] = "password123"; // Placeholder for demonstration

    printf("Username: ");
    scanf("%255s", username);

    printf("Password: ");
    disable_echo();
    scanf("%255s", password);
    enable_echo();
    printf("\n");

    if (strcmp(password, correct_password) == 0) {
        printf("Authentication successful. Welcome, %s!\n", username);
        *authenticated = 1;
    } else {
        printf("Authentication failed.\n");
        *authenticated = 0;
    }
}

void allocate_memory(int size) {
    if (size <= 0 || size > MEMORY_SIZE) {
        printf("Invalid memory size.\n");
        return;
    }

    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i].is_free && memory[i].size >= size) {
            memory[i].is_free = 0;
            printf("Allocated %d units of memory at block %d.\n", size, i);
            return;
        }
    }

    if (memory_blocks < MEMORY_SIZE) {
        memory[memory_blocks].size = size;
        memory[memory_blocks].is_free = 0;
        memory[memory_blocks].start_address = memory_blocks * size;
        printf("Allocated %d units of memory at block %d.\n", size, memory_blocks);
        memory_blocks++;
    } else {
        printf("Memory allocation failed. Not enough memory.\n");
    }
}

void deallocate_memory(int block_number) {
    if (block_number < 0 || block_number >= memory_blocks) {
        printf("Invalid memory block number.\n");
        return;
    }

    if (memory[block_number].is_free) {
        printf("Memory block %d is already free.\n", block_number);
    } else {
        memory[block_number].is_free = 1;
        printf("Freed memory block %d.\n", block_number);
    }
}

void display_memory() {
    printf("Memory Blocks:\n");
    printf("Block\tSize\tStatus\n");
    for (int i = 0; i < memory_blocks; i++) {
        printf("%d\t%d\t%s\n", i, memory[i].size, memory[i].is_free ? "Free" : "Allocated");
    }
}

void error(const char *msg) {
    fprintf(stderr, "Error: %s (%s)\n", msg, strerror(errno));
}

void flush_input() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void disable_echo() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void enable_echo() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}