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
#include <fcntl.h>
#include <string.h>

#define KEY_MSG 1234
#define KEY_SHM 5678
#define KEY_SEM 9012

#define MSG_TYPE_LOGIN 1
#define MSG_TYPE_ACTION 2
#define MSG_TYPE_UPDATE 3

#define UNIT_LIGHT 0
#define UNIT_HEAVY 1
#define UNIT_CAVALRY 2
#define UNIT_WORKER 3

#define COST_LIGHT 100
#define COST_HEAVY 250
#define COST_CAVALRY 550
#define COST_WORKER 150

#define TIME_LIGHT 2
#define TIME_HEAVY 3
#define TIME_CAVALRY 5
#define TIME_WORKER 2

struct UnitStats {
    int cost;
    float attack;
    float defense;
    int build_time;
};

struct PlayerState {
    int resources;
    int units[4];
    int production_queue[4];
    int production_timer[4];
    int victory_points;
    int id;
};

struct GameState {
    struct PlayerState p1;
    struct PlayerState p2;
    int connected_clients;
    char last_event_msg[100];
};

struct MsgBuf {
    long mtype;
    int player_id;
    int command; 
    int unit_type;
    int count;
    int units_to_attack[3];
};

struct UpdateMsg {
    long mtype;
    struct PlayerState self;
    struct PlayerState enemy;
    char message[100];
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int my_strlen(const char *str) {
    int i = 0;
    while(str[i] != '\0') i++;
    return i;
}

void my_print(const char *str) {
    write(1, str, my_strlen(str));
}

void my_itoa(int n, char *s) {
    int i = 0, j = 0, sign = n;
    if ((sign = n) < 0) n = -n;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    char c;
    for (j = 0; j < i / 2; j++) {
        c = s[j];
        s[j] = s[i - 1 - j];
        s[i - 1 - j] = c;
    }
}

void my_print_int(int n) {
    char buf[16];
    my_itoa(n, buf);
    my_print(buf);
}

int my_atoi(const char *str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9')
            res = res * 10 + str[i] - '0';
    }
    return res;
}

#endif