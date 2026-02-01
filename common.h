#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#define KEY_SHM 11111
#define KEY_MSG 22222
#define KEY_SEM 33333

#define U_LIGHT 0
#define U_HEAVY 1
#define U_CAV 2
#define U_WORK 3

struct UnitStats {
    int cost;
    int attack; 
    int defense; 
    int time;
};

struct TrainTask {
    int unit_type;
    int count;
    int next_tick;
    int active;
};

struct Player {
    int id;
    int gold;
    int units[4];
    struct TrainTask training;
    int points;
};

struct Battle {
    int active;
    int tick_end;
    int attacker_id;
    int units[4];
};

struct CommandSlot {
    int active;
    int player_id;
    char type; 
    int u_type;
    int count;
};

struct GameState {
    long tick;
    struct Player p1;
    struct Player p2;
    struct Battle battle;
    struct CommandSlot cmd_slot;
};

struct MsgBuf {
    long mtype;
    int player_id;
    char cmd_type;
    int u_type;
    int count;
    char raw_text[100];
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif