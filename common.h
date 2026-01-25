#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <math.h>

#define PROJECT_ID 'Z'
#define MAX_TEXT 512

#define LIGHT 0
#define HEAVY 1
#define CAVALRY 2
#define WORKER 3

static const int COST[] = {100, 250, 550, 150};
static const double ATTACK_VAL[] = {1.0, 1.5, 3.5, 0.0};
static const double DEFENSE_VAL[] = {1.2, 3.0, 1.2, 0.0};
static const int TIME_VAL[] = {2, 3, 5, 2};

#define MSG_LOGIN 1
#define MSG_BUILD 2
#define MSG_ATTACK 3
#define MSG_UPDATE 10
#define MSG_RESULT 11

typedef struct {
    int pid;
    int is_active;
    double gold;
    int units[4];
    int score;
} Player;

typedef struct {
    Player players[2];
} GameState;

typedef struct {
    long mtype;
    int source_id;
    int cmd_type;
    int unit_type;
    int count;
    int attack_army[4];
    int gold_now;
    int units_now[4];
    int score_me;
    int score_enemy;
    char text[MAX_TEXT];
} GameMsg;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif