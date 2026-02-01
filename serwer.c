#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

int semid, shmid, msgid;
struct StanGry *gra;

// Operacje na semaforze
void sem_p() {
    struct sembuf buf;
    buf.sem_num = 0;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);
}

void sem_v() {
    struct sembuf buf;
    buf.sem_num = 0;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    semop(semid, &buf, 1);
}

void czyszczenie(int sig) {
    shmdt(gra);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
    exit(0);
}

void pisz(const char* s) {
    write(1, s, strlen(s));
}

void obsluga_ataku(int id_atakujacego, int L, int C, int J) {
    int id_obroncy = -1;
    for(int i=0; i<2; i++) {
        if (gra->gracze[i].id != id_atakujacego && gra->gracze[i].aktywny) {
            id_obroncy = i;
            break;
        }
    }
    
    if (id_obroncy == -1) return;

    int idx_atk = (gra->gracze[0].id == id_atakujacego) ? 0 : 1;
    struct GraczInfo *ATK = &gra->gracze[idx_atk];
    struct GraczInfo *DEF = &gra->gracze[id_obroncy];

    // Sprawdzenie czy atakujący ma jednostki
    if (ATK->jednostki[PIECHOTA_LEKKA] < L ||
        ATK->jednostki[PIECHOTA_CIEZKA] < C ||
        ATK->jednostki[JAZDA] < J) {
        return; 
    }

    // Obliczenia siły (wg PDF)
    double Sa = L * ATAK[0] + C * ATAK[1] + J * ATAK[2];
    double Sb = DEF->jednostki[0] * OBRONA[0] + 
                DEF->jednostki[1] * OBRONA[1] + 
                DEF->jednostki[2] * OBRONA[2];

    if (Sb == 0) Sb = 1.0;

    int win = 0;
    // Straty obrońcy
    if (Sa > Sb) {
        for(int i=0; i<4; i++) DEF->jednostki[i] = 0;
        ATK->punkty++;
        win = 1;
    } else {
        for(int i=0; i<3; i++) {
             int utrata = (int)((double)DEF->jednostki[i] * Sa / Sb);
             DEF->jednostki[i] -= utrata;
             if (DEF->jednostki[i] < 0) DEF->jednostki[i] = 0;
        }
    }

    // Straty atakującego (kontratak)
    double Sa_def = L * OBRONA[0] + C * OBRONA[1] + J * OBRONA[2];
    double Sb_atk_val = DEF->jednostki[0] * ATAK[0] + DEF->jednostki[1] * ATAK[1] + DEF->jednostki[2] * ATAK[2]; 
    
    if (Sa_def > 0) {
         // Uproszczony algorytm strat dla atakującego
         int str_l = (int)((double)L * Sb_atk_val / Sa_def);
         int str_c = (int)((double)C * Sb_atk_val / Sa_def);
         int str_j = (int)((double)J * Sb_atk_val / Sa_def);
         
         if (str_l > L) str_l = L;
         if (str_c > C) str_c = C;
         if (str_j > J) str_j = J;

         ATK->jednostki[0] -= str_l;
         ATK->jednostki[1] -= str_c;
         ATK->jednostki[2] -= str_j;
    }

    // Budowanie komunikatu bez sprintf
    // Poniewaz komunikat w pamieci to char[], uzywamy strcpy
    if (win) {
        // "Atak udany!"
        char msg[] = "Atak udany! Zdobyles punkt.";
        int len = strlen(msg);
        for(int k=0; k<len && k<255; k++) gra->ostatni_komunikat[k] = msg[k];
        gra->ostatni_komunikat[len] = 0;
    } else {
        // "Atak odparty."
        char msg[] = "Atak odparty. Brak punktu.";
        int len = strlen(msg);
        for(int k=0; k<len && k<255; k++) gra->ostatni_komunikat[k] = msg[k];
        gra->ostatni_komunikat[len] = 0;
    }
}

int main() {
    signal(SIGINT, czyszczenie); //

    // 1. Zasoby
    shmid = shmget(KLUCZ_SHM, sizeof(struct StanGry), IPC_CREAT | 0600);
    gra = (struct StanGry*)shmat(shmid, NULL, 0);
    msgid = msgget(KLUCZ_MSG, IPC_CREAT | 0600);
    semid = semget(KLUCZ_SEM, 1, IPC_CREAT | 0600);

    semctl(semid, 0, SETVAL, 1);
    memset(gra, 0, sizeof(struct StanGry));
    
    pisz("Serwer uruchomiony...\n");

    if (fork() == 0) {
        // PROCES CZASU (Zasoby, Produkcja)
        struct msgupdate upd;
        upd.mtype = MSG_UPDATE;

        while(1) {
            sleep(1); 
            
            sem_p(); // LOCK

            for(int i=0; i<2; i++) {
                if (!gra->gracze[i].aktywny) continue;

                // Surowce
                int workers = gra->gracze[i].jednostki[ROBOTNICY];
                gra->gracze[i].surowce += 50 + (workers * 5);

                // Produkcja
                for(int u=0; u<4; u++) {
                    if (gra->gracze[i].w_produkcji[u] > 0) {
                        gra->gracze[i].czas_produkcji[u]--;
                        if (gra->gracze[i].czas_produkcji[u] <= 0) {
                            gra->gracze[i].jednostki[u]++;
                            gra->gracze[i].w_produkcji[u]--;
                            if (gra->gracze[i].w_produkcji[u] > 0)
                                gra->gracze[i].czas_produkcji[u] = CZAS_PROD[u];
                        }
                    }
                }
            }

            // Wysyłanie stanu
            if (gra->licznik_graczy > 0) {
                // Kopiowanie wiadomosci
                int len = strlen(gra->ostatni_komunikat);
                for(int k=0; k<=len; k++) upd.wiadomosc[k] = gra->ostatni_komunikat[k];

                if (gra->gracze[0].aktywny) {
                    upd.mtype = 10 + gra->gracze[0].id;
                    upd.ja = gra->gracze[0];
                    upd.wrog = gra->gracze[1];
                    msgsnd(msgid, &upd, sizeof(upd)-sizeof(long), IPC_NOWAIT);
                }
                if (gra->gracze[1].aktywny) {
                    upd.mtype = 10 + gra->gracze[1].id;
                    upd.ja = gra->gracze[1];
                    upd.wrog = gra->gracze[0];
                    msgsnd(msgid, &upd, sizeof(upd)-sizeof(long), IPC_NOWAIT);
                }
            }
            sem_v(); // UNLOCK
        }
    } else {
        // PROCES ODBIORU KOMUNIKATÓW
        struct msgbuf buf;
        while(1) {
            msgrcv(msgid, &buf, sizeof(buf)-sizeof(long), 0, 0);
            
            sem_p(); // LOCK

            if (buf.mtype == MSG_LOGIN) {
                if (gra->licznik_graczy < 2) {
                    int idx = gra->licznik_graczy;
                    gra->gracze[idx].id = buf.id_nadawcy;
                    gra->gracze[idx].aktywny = 1;
                    gra->gracze[idx].surowce = 300;
                    gra->licznik_graczy++;
                    pisz("Nowy gracz.\n");
                }
            } 
            else if (buf.mtype == MSG_AKCJA) {
                int idx = -1;
                if (gra->gracze[0].id == buf.id_nadawcy) idx = 0;
                else if (gra->gracze[1].id == buf.id_nadawcy) idx = 1;

                if (idx != -1) {
                    if (buf.typ_akcji == 1) { // Buduj
                        int typ = buf.parametry[0];
                        int ilosc = buf.parametry[1];
                        int koszt = KOSZT[typ] * ilosc;
                        
                        if (gra->gracze[idx].surowce >= koszt) {
                            gra->gracze[idx].surowce -= koszt;
                            if (gra->gracze[idx].w_produkcji[typ] == 0)
                                gra->gracze[idx].czas_produkcji[typ] = CZAS_PROD[typ];
                            gra->gracze[idx].w_produkcji[typ] += ilosc;
                        }
                    } 
                    else if (buf.typ_akcji == 2) { // Atak
                        obsluga_ataku(buf.id_nadawcy, buf.parametry[0], buf.parametry[1], buf.parametry[2]);
                    }
                }
            }
            sem_v(); // UNLOCK
        }
    }
}