#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <sys/select.h>
#include <termios.h>

#define KEY_MSG 1234
#define KEY_SHM 5678
#define KEY_SEM 9012

#define MAX_TEXT 512

#define UNIT_LIGHT 0
#define UNIT_HEAVY 1
#define UNIT_CAVALRY 2
#define UNIT_WORKER 3

typedef struct {
    int cost;
    double attack;
    double defense;
    int time;
} UnitStats;

static const UnitStats UNITS[4] = {
    {100, 1.0, 1.2, 2},
    {250, 1.5, 3.0, 3},
    {550, 3.5, 1.2, 5},
    {150, 0.0, 0.0, 2}
};

typedef struct {
    long type;
    int source_id;
    int cmd;
    int args[5];
    char text[MAX_TEXT];
} Message;

typedef struct {
    int type;
    int count;
    int time_left;
} ProductionTask;

typedef struct {
    int resources;
    int units[4];
    int wins;
    int workers_active;
    ProductionTask production_queue[10];
    int q_size;
} Player;

typedef struct {
    Player players[2];
    int connected_clients;
    int game_over;
} GameState;

#define CMD_LOGIN 1
#define CMD_TRAIN 2
#define CMD_ATTACK 3
#define CMD_UPDATE 4
#define CMD_RESULT 5

#endif