#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define PROJECT_KEY 12345
#define MAX_PLAYERS 2
#define WIN_THRESHOLD 5

// Typy akcji
#define ACTION_LOGIN 1
#define ACTION_STATE 2
#define ACTION_BUILD 3
#define ACTION_ATTACK 4

typedef struct {
    int cost;
    float attack_val;
    float defense_val;
    int build_time;
    char name[32];
} UnitData;

static const UnitData UNIT_STATS[4] = {
    {100, 1.0, 1.2, 2, "Lekka Piechota "},
    {250, 1.5, 3.0, 3, "Ciezka Piechota"},
    {550, 3.5, 1.2, 5, "Jazda          "},
    {150, 0.0, 0.0, 2, "Robotnicy      "}
};

typedef struct {
    long mtype;
    int player_id;
    int action;
    int unit_type;
    int quantity;
    int army_info[4];
    int resource_info;
    int score_info;
    int game_status; 
} GameMessage;

typedef struct {
    int player_resources[MAX_PLAYERS];
    int player_army[MAX_PLAYERS][4];
    int player_scores[MAX_PLAYERS];
    int is_game_started;
} SharedGameState;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// --- FUNKCJE POMOCNICZE I/O ---

static inline void write_str(int fd, const char *s) {
    write(fd, s, strlen(s));
}

// Zamiana int na string (zamiast sprintf)
static inline void write_int(int fd, int n) {
    if (n == 0) { write(fd, "0", 1); return; }
    if (n < 0) { write(fd, "-", 1); n = -n; }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) write(fd, &buf[--i], 1);
}

// Zamiana string na int (zamiast atoi)
static inline int string_to_int(const char *s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

// --- SEMAFORY ---

static inline void lock_shm(int semid) {
    struct sembuf op = {0, -1, SEM_UNDO};
    semop(semid, &op, 1);
}

static inline void unlock_shm(int semid) {
    struct sembuf op = {0, 1, SEM_UNDO};
    semop(semid, &op, 1);
}

#endif