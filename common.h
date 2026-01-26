#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define KLUCZ 54321
#define MAX_GRACZY 2

#define MSG_LOGIN 1
#define MSG_STAN  2
#define MSG_BUDUJ 3
#define MSG_ATAK  4
#define MSG_WYNIK 5
#define MSG_START 6

#define LEKKA     0
#define CIEZKA    1
#define JAZDA     2
#define ROBOTNICY 3

typedef struct {
    int cena;
    float atak;
    float obrona;
    int czas_produkcji;
} JednostkaInfo;

static const JednostkaInfo SPECYFIKACJA[4] = {
    {100, 1.0, 1.2, 2},
    {250, 1.5, 3.0, 3},
    {550, 3.5, 1.2, 5},
    {150, 0.0, 0.0, 2}
};

typedef struct {
    long mtype;
    int id_nadawcy;
    int akcja;
    int dane[4];
    int wartosc;
    int punkty;
} Komunikat;

typedef struct {
    int surowce[2];
    int armia[2][4];
    int punkty[2];
    int liczba_graczy;
    int gra_aktywna;
} StanGry;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif