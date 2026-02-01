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

// Kanaly wiadomosci
#define CHAN_SERVER 100
#define CHAN_P1     101
#define CHAN_P2     102

// Akcje
#define ACT_LOGIN  1
#define ACT_STATE  2
#define ACT_BUILD  3
#define ACT_ATTACK 4

typedef struct {
    int cost;
    float atk;
    float def;
    int time;
    char name[20];
} Unit;

static const Unit STATS[4] = {
    {100, 1.0, 1.2, 2, "Lekka Piechota"},
    {250, 1.5, 3.0, 3, "Ciezka Piechota"},
    {550, 3.5, 1.2, 5, "Jazda         "},
    {150, 0.0, 0.0, 2, "Robotnicy     "}
};

typedef struct {
    long mtype;
    int sender_pid;
    int player_id;
    int action;
    int u_type;
    int u_count;
    int army[4];
    int gold;
    int score;
    int active;
} Msg;

typedef struct {
    int gold[MAX_PLAYERS];
    int army[MAX_PLAYERS][4];
    int score[MAX_PLAYERS];
    int running;
} GameData;

union semun { int val; };

// Pomocnicze funkcje I/O na deskryptorach
static inline void print_s(const char *s) { write(STDOUT_FILENO, s, strlen(s)); }

static inline void print_i(int n) {
    if (n == 0) { write(STDOUT_FILENO, "0", 1); return; }
    if (n < 0) { write(STDOUT_FILENO, "-", 1); n = -n; }
    char b[12]; int i = 0;
    while(n > 0) { b[i++] = (n % 10) + '0'; n /= 10; }
    while(i > 0) write(STDOUT_FILENO, &b[--i], 1);
}

static inline int str_to_i(char *s) {
    int res = 0;
    while(*s >= '0' && *s <= '9') { res = res * 10 + (*s - '0'); s++; }
    return res;
}

// Obsluga semafora
static inline void lock(int id) { struct sembuf o = {0, -1, SEM_UNDO}; semop(id, &o, 1); }
static inline void unlock(int id) { struct sembuf o = {0, 1, SEM_UNDO}; semop(id, &o, 1); }

#endif