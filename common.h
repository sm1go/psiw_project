#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define KEY 12345
#define MAX_PLAYERS 2
#define WIN_POINTS 5

// Message Types
#define M_LOGIN    10
#define M_STATE    11
#define M_BUILD    12
#define M_ATTACK   13
#define M_RESULT   14

// Unit Indices
#define LIGHT_INF  0
#define HEAVY_INF  1
#define CAVALRY    2
#define WORKER     3

typedef struct {
    int price;
    float attack;
    float defense;
    int build_time;
    char name[32];
} UnitStats;

// Statystyki jednostek zgodnie z tabelą
static const UnitStats UNITS[4] = {
    {100, 1.0, 1.2, 2, "Lekka Piechota "},
    {250, 1.5, 3.0, 3, "Ciezka Piechota"},
    {550, 3.5, 1.2, 5, "Jazda          "},
    {150, 0.0, 0.0, 2, "Robotnicy      "}
};

typedef struct {
    long mtype;
    int player_id;
    int action_type;
    int unit_idx;
    int count;
    int army[4];
    int resources;
    int points;
    int status; 
} GameMsg;

typedef struct {
    int resources[MAX_PLAYERS];
    int army[MAX_PLAYERS][4];
    int points[MAX_PLAYERS];
    int game_active;
} GameState;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Funkcje pomocnicze dla jedynego semafora
static inline void lock_sync(int semid) {
    struct sembuf op = {0, -1, SEM_UNDO};
    semop(semid, &op, 1);
}

static inline void unlock_sync(int semid) {
    struct sembuf op = {0, 1, SEM_UNDO};
    semop(semid, &op, 1);
}

#endif