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

#define KLUCZ 12345
#define SHM_KLUCZ 12346
#define SEM_KLUCZ 12347

#define MSG_LOGOWANIE 10
#define MSG_BUDUJ     11
#define MSG_ATAK      12
#define MSG_STAN      13
#define MSG_START     15

// Struktura komunikatu MQ
typedef struct {
    long mtype;
    int id_gracza;
    int wybor;
    int jednostki[4]; // 0:Lekka, 1:Ciezka, 2:Jazda, 3:Robotnicy
    int zasoby;
    int punkty;
} MsgBuf;

// Stan pojedynczego gracza w pamieci wspoldzielonej
typedef struct {
    int active;
    int resources;
    int units[4];
    int training_type;   // -1 jesli brak
    int training_count;
    int training_time_left;
    int points;
    int is_attacking;
    int att_units[4];
    int att_time_left;
} PlayerState;

typedef struct {
    PlayerState p[2];
} GameState;

// Statystyki (wartosci * 10 dla unikniecia float zgodnie z plikami)
static const int COSTS[] = {100, 250, 550, 150};
static const int TIMES[] = {2, 3, 5, 2};
static const int ATK_VAL[] = {10, 15, 35, 0}; // x10
static const int DEF_VAL[] = {12, 30, 12, 0}; // x10

#endif