

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>








#define PROJECT_ID 'Z'

#define LIGHT 0
#define HEAVY 1
#define CAVALRY 2
#define WORKER 3

[cite_start]

typedef struct 
{
    int pid;
    int is_active;
    int gold;
    int units[4];
    int score;

} Player;

typedef struct
{
    Player players[2];
} GameStats;

[cite_start]

typedef struct 
{
    long mtype;
    int source_id;
    int cmd_type;
    int unit_type;
    int count;

    char text[];
};




