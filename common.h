#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define KEY_ID 12345
#define MAX_QUEUE 10
#define UNIT_LIGHT 0
#define UNIT_HEAVY 1
#define UNIT_CAVALRY 2
#define UNIT_WORKER 3

#define CMD_LOGIN 1
#define CMD_BUILD 2
#define CMD_ATTACK 3
#define MSG_STATE 4
#define MSG_RESULT 5
#define MSG_GAME_OVER 6

typedef struct {
    int id;
    int gold;
    int units[4];
    int wins;
    int worker_count;
} PlayerData;

typedef struct {
    int type;
    int count;
    int time_left;
} BuildTask;

typedef struct {
    PlayerData players[2];
    BuildTask build_queue[2][MAX_QUEUE];
    int queue_head[2];
    int queue_tail[2];
    int connected_clients;
    pid_t client_pids[2];
} GameState;

typedef struct {
    long mtype;
    int src_id;
    int cmd;
    int args[4];
    char text[256];
} Message;

#endif