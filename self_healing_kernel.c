#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#define NUM_SUBSYSTEMS 5
#define MAX_LOG_SIZE 16384 // 16 KB per log file

typedef enum { HEALTHY, FAILED, RECOVERING } status_t;

typedef struct {
    char name[20];
    status_t status;
    int health;
    int restart_count;
} subsystem_t;

static subsystem_t subsystems[NUM_SUBSYSTEMS];
static char MANUAL_LOG[] = "manual_kernel_log.txt";
static char AUTO_LOG[]   = "auto_kernel_log.txt";

void now_str(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tm);
}

void rotate_if_needed(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0 && st.st_size > MAX_LOG_SIZE) {
        char oldname[64];
        snprintf(oldname, sizeof(oldname), "%s.old", filename);
        rename(filename, oldname);
        FILE *f = fopen(filename, "w");
        if (f) {
            char ts[32];
            now_str(ts, sizeof(ts));
            fprintf(f, "[%s] [INFO] Log rotated. Old log saved as %s\n", ts, oldname);
            fclose(f);
        }
    }
}

void log_event(const char *filename, const char *level, const char *msg) {
    FILE *f = fopen(filename, "a");
    if (!f) return;
    char ts[32]; now_str(ts, sizeof(ts));
    fprintf(f, "[%s] [%s] %s\n", ts, level, msg);
    fclose(f);
    rotate_if_needed(filename);
}

// ---------- Subsystem Management ----------
void init_subsystems() {
    const char *names[NUM_SUBSYSTEMS] = {"CPU", "Memory", "I/O", "Network", "Storage"};
    for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
        strcpy(subsystems[i].name, names[i]);
        subsystems[i].status = HEALTHY;
        subsystems[i].health = 100;
        subsystems[i].restart_count = 0;
    }
}

void show_status() {
    printf("\nSubsystem Status\n--------------------------------\n");
    for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
        const char *state = (subsystems[i].status == HEALTHY) ? "HEALTHY" :
                            (subsystems[i].status == FAILED) ? "FAILED" : "RECOVERING";
        printf("%d) %-8s | %-10s | Health: %3d%% | Restarts: %d\n",
               i+1, subsystems[i].name, state, subsystems[i].health, subsystems[i].restart_count);
    }
    printf("--------------------------------\n\n");
}

void crash_subsystem(int id, const char *logfile) {
    if (id < 1 || id > NUM_SUBSYSTEMS) return;
    subsystem_t *ss = &subsystems[id-1];
    if (ss->status != FAILED) {
        ss->status = FAILED;
        ss->health = 0;
        char msg[128];
        sprintf(msg, "Subsystem %s crashed.", ss->name);
        log_event(logfile, "WARNING", msg);
        printf("%s crashed.\n", ss->name);
    }
}

void heal_subsystem(int id, const char *logfile) {
    if (id < 1 || id > NUM_SUBSYSTEMS) return;
    subsystem_t *ss = &subsystems[id-1];
    if (ss->status == FAILED) {
        ss->status = RECOVERING;
        char msg[128];
        sprintf(msg, "Healing subsystem %s...", ss->name);
        log_event(logfile, "INFO", msg);
        usleep(400000);
        ss->status = HEALTHY;
        ss->health = 100;
        sprintf(msg, "Subsystem %s healed successfully.", ss->name);
        log_event(logfile, "SUCCESS", msg);
        printf("%s healed successfully.\n", ss->name);
    }
}

void restart_subsystem(int id, const char *logfile) {
    if (id < 1 || id > NUM_SUBSYSTEMS) return;
    subsystem_t *ss = &subsystems[id-1];
    ss->status = RECOVERING;
    char msg[128];
    sprintf(msg, "Restarting subsystem %s...", ss->name);
    log_event(logfile, "INFO", msg);
    usleep(400000);
    ss->status = HEALTHY;
    ss->health = 100;
    ss->restart_count++;
    sprintf(msg, "Subsystem %s restarted successfully.", ss->name);
    log_event(logfile, "SUCCESS", msg);
    printf("%s restarted successfully.\n", ss->name);
}

// ---------- Automatic Mode ----------
void simulate_failures(const char *logfile) {
    if (rand() % 100 < 20) {
        int i = rand() % NUM_SUBSYSTEMS;
        if (subsystems[i].status == HEALTHY) {
            crash_subsystem(i+1, logfile);
        }
    }
}

void auto_heal(const char *logfile) {
    for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
        if (subsystems[i].status == FAILED) {
            heal_subsystem(i+1, logfile);
        }
    }
}

int automatic_mode() {
    log_event(AUTO_LOG, "INFO", "Automatic mode started.");
    printf("Automatic mode running... type 'exit' to return.\n");

    char input[10];
    while (1) {
        simulate_failures(AUTO_LOG);
        auto_heal(AUTO_LOG);
        sleep(1);

        // check for exit request
        printf("(auto)> ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin)) {
            if (strncmp(input, "exit", 4) == 0)
                break;
        }
    }

    log_event(AUTO_LOG, "INFO", "Exited automatic mode.");
    return 0;
}

// ---------- Manual Mode ----------
int manual_mode() {
    log_event(MANUAL_LOG, "INFO", "Manual mode started.");
    char command[64];

    while (1) {
        printf("kernel(manual)> ");
        fflush(stdout);

        if (!fgets(command, sizeof(command), stdin)) break;
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "status") == 0) {
            show_status();
        } else if (strncmp(command, "crash ", 6) == 0) {
            crash_subsystem(atoi(&command[6]), MANUAL_LOG);
        } else if (strncmp(command, "heal ", 5) == 0) {
            heal_subsystem(atoi(&command[5]), MANUAL_LOG);
        } else if (strncmp(command, "restart ", 8) == 0) {
            restart_subsystem(atoi(&command[8]), MANUAL_LOG);
        } else if (strcmp(command, "exit") == 0) {
            log_event(MANUAL_LOG, "INFO", "Exited manual mode.");
            return 0;
        } else if (strcmp(command, "help") == 0) {
            printf("Commands: status | crash <id> | heal <id> | restart <id> | exit\n");
        } else {
            printf("Unknown command. Type 'help'.\n");
        }
    }
    return 0;
}

int main() {
    srand(time(NULL));
    init_subsystems();

    while (1) {
        printf("\nSelect Mode:\n");
        printf("  1. Manual\n");
        printf("  2. Automatic\n");
        printf("  3. Exit\n");
        printf("Choice: ");
        fflush(stdout);

        int choice;
        if (scanf("%d%*c", &choice) != 1) {
            printf("Invalid input.\n");
            return 0;
        }

        if (choice == 1) {
            manual_mode();
        } else if (choice == 2) {
            automatic_mode();
        } else if (choice == 3) {
            printf("Exiting kernel simulator.\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    return 0;
}
