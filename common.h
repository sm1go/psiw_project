#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define KEY_MSG 1234
#define KEY_SHM 5678
#define KEY_SEM 9012

// Jednostki
#define UNIT_L 0 // Lekka
#define UNIT_H 1 // Cieżka
#define UNIT_J 2 // Jazda
#define UNIT_R 3 // Robotnik

// Typy komunikatów
#define MSG_LOGIN 1
#define MSG_CMD   2
#define MSG_STATE 3

// Struktura wiadomości w kolejce
typedef struct {
    long mtype;
    int client_id; // 1 lub 2
    char cmd[10];  // TRAIN, ATTACK
    int unit_type;
    int count;
    char text[512]; // Do przesyłania stanu gry jako tekst
} msg_buf;

// Struktura pamięci współdzielonej (stan gry)
typedef struct {
    int resources[3];      // resources[1] dla gracza 1, [2] dla gracza 2
    int units[3][4];       // units[gracz][typ]
    int production_queue[3][4]; // ile jednostek się produkuje
    int prod_timer[3][4];       // czas do wyprodukowania kolejnej
    int score[3];
    int connected_clients;
} GameState;

// Funkcje pomocnicze do wypisywania (zamiast printf)
void my_print(const char *s) {
    write(1, s, strlen(s));
}

void my_print_int(int n) {
    char buf[20];
    int i = 0;
    if (n == 0) {
        write(1, "0", 1);
        return;
    }
    int neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = tmp;
    }
    write(1, buf, i);
}

// Konwersja string na int (prosty atoi)
int my_atoi(char *str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9')
            res = res * 10 + str[i] - '0';
    }
    return res;
}

#endif