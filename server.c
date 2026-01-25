#include "common.h"
#include <math.h>
#include <sys/wait.h>

int msg_id, shm_id, sem_id;
StanGry *stan;

void P(int sem_id) {
    struct sembuf buf;
    buf.sem_num = 0;
    buf.sem_op = -1;
    buf.sem_flg = SEM_UNDO;
    semop(sem_id, &buf, 1);
}

void V(int sem_id) {
    struct sembuf buf;
    buf.sem_num = 0;
    buf.sem_op = 1;
    buf.sem_flg = SEM_UNDO;
    semop(sem_id, &buf, 1);
}

void cleanup(int sig) {
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    exit(0);
}

float oblicz_sile(int armia[], int tryb_atak) {
    float suma = 0.0;
    for(int i=0; i<4; i++) {
        if(tryb_atak) suma += armia[i] * SPECYFIKACJA[i].atak;
        else          suma += armia[i] * SPECYFIKACJA[i].obrona;
    }
    return suma;
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGCHLD, SIG_IGN); 

    key_t klucz = KLUCZ;
    
    msg_id = msgget(klucz, IPC_CREAT | 0600);
    if (msg_id == -1) { perror("msgget"); exit(1); }

    shm_id = shmget(klucz, sizeof(StanGry), IPC_CREAT | 0600);
    if (shm_id == -1) { perror("shmget"); exit(1); }
    stan = (StanGry*)shmat(shm_id, NULL, 0);

    sem_id = semget(klucz, 1, IPC_CREAT | 0600);
    union semun arg;
    arg.val = 1;
    semctl(sem_id, 0, SETVAL, arg);

    memset(stan, 0, sizeof(StanGry));
    stan->surowce[0] = 300;
    stan->surowce[1] = 300;

    printf("Serwer uruchomiony.\n");

    if (fork() == 0) {
        while(1) {
            sleep(1);
            P(sem_id);
            if (stan->gra_aktywna) {
                stan->surowce[0] += 50 + (stan->armia[0][ROBOTNICY] * 5);
                stan->surowce[1] += 50 + (stan->armia[1][ROBOTNICY] * 5);
            }
            V(sem_id);
        }
    }

    Komunikat msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 10, 0) == -1) continue;

        int id = msg.id_nadawcy;

        switch(msg.akcja) {
            case MSG_LOGIN: {
                P(sem_id);
                if (stan->liczba_graczy < 2) {
                    msg.id_nadawcy = stan->liczba_graczy;
                    stan->liczba_graczy++;
                    if (stan->liczba_graczy == 2) {
                        stan->gra_aktywna = 1;
                        printf("Gra startuje!\n");
                    }
                    msg.wartosc = 1;
                } else {
                    msg.wartosc = 0;
                }
                V(sem_id);
                
                msg.mtype = msg.dane[0]; 
                msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0);
                break;
            }

            case MSG_STAN: {
                P(sem_id);
                msg.wartosc = stan->surowce[id];
                memcpy(msg.dane, stan->armia[id], sizeof(int)*4);
                
                int wygrana = 0;
                if (stan->punkty[id] >= 5) wygrana = 1;
                else if (stan->punkty[(id+1)%2] >= 5) wygrana = 2;
                V(sem_id);

                msg.akcja = wygrana;
                msg.mtype = id + 20;
                msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0);
                break;
            }

            case MSG_BUDUJ: {
                int typ = msg.dane[0];
                int ilosc = msg.dane[1];
                int koszt = SPECYFIKACJA[typ].cena * ilosc;
                int czas = SPECYFIKACJA[typ].czas_produkcji;

                P(sem_id);
                if (stan->surowce[id] >= koszt) {
                    stan->surowce[id] -= koszt;
                    V(sem_id);

                    if (fork() == 0) {
                        for (int k=0; k<ilosc; k++) {
                            sleep(czas);
                            P(sem_id);
                            stan->armia[id][typ]++;
                            V(sem_id);
                        }
                        exit(0);
                    }
                } else {
                    V(sem_id);
                }
                break;
            }

            case MSG_ATAK: {
                int atakujace[4];
                memcpy(atakujace, msg.dane, sizeof(int)*4);
                int wrog = (id + 1) % 2;

                P(sem_id);
                int ma_wojsko = 1;
                for(int i=0; i<3; i++) {
                    if (stan->armia[id][i] < atakujace[i]) ma_wojsko = 0;
                }

                if (ma_wojsko) {
                    for(int i=0; i<3; i++) stan->armia[id][i] -= atakujace[i];
                    
                    V(sem_id);

                    if (fork() == 0) {
                        sleep(5);

                        P(sem_id);
                        int armia_obroncy[4];
                        for(int i=0; i<4; i++) armia_obroncy[i] = stan->armia[wrog][i];

                        float SA = oblicz_sile(atakujace, 1);
                        float SB = oblicz_sile(armia_obroncy, 0);

                        if (SA > SB) {
                            stan->punkty[id]++;
                            for(int i=0; i<4; i++) stan->armia[wrog][i] = 0;
                        } else {
                            float ratio = (SB > 0) ? (SA / SB) : 0;
                            for(int i=0; i<4; i++) {
                                int straty = floor(stan->armia[wrog][i] * ratio);
                                stan->armia[wrog][i] -= straty;
                                if (stan->armia[wrog][i] < 0) stan->armia[wrog][i] = 0;
                            }
                        }

                        float SB_atak = oblicz_sile(armia_obroncy, 1);
                        float SA_obrona = oblicz_sile(atakujace, 0);

                        if (SB_atak > SA_obrona) {
                            for(int i=0; i<4; i++) atakujace[i] = 0;
                        } else {
                            float ratio = (SA_obrona > 0) ? (SB_atak / SA_obrona) : 0;
                            for(int i=0; i<3; i++) {
                                int straty = floor(atakujace[i] * ratio);
                                atakujace[i] -= straty;
                                if (atakujace[i] < 0) atakujace[i] = 0;
                            }
                        }

                        for(int i=0; i<3; i++) stan->armia[id][i] += atakujace[i];

                        V(sem_id);
                        exit(0);
                    }
                } else {
                    V(sem_id);
                }
                break;
            }
        }
    }
    return 0;
}