#include "common.h"

int msg_id, shm_id, sem_id;
GameState *game;

// Operacje na semaforze [cite: 254, 257]
void sem_lock() {
    struct sembuf sb = {0, -1, 0};
    semop(sem_id, &sb, 1);
}
void sem_unlock() {
    struct sembuf sb = {0, 1, 0};
    semop(sem_id, &sb, 1);
}

void oblicz_walke(int atk_idx) {
    int def_idx = 1 - atk_idx;
    long sa = 0, sb = 0;
    
    // Sila ataku agresora
    for(int i=0; i<3; i++) sa += game->p[atk_idx].att_units[i] * ATK_VAL[i];
    // Sila obrony ofiary (tylko jednostki w bazie)
    for(int i=0; i<3; i++) sb += game->p[def_idx].units[i] * DEF_VAL[i];

    if (sa > sb) game->p[atk_idx].points++;

    // Straty obroncy
    if (sa >= sb && sb > 0) {
        for(int i=0; i<4; i++) game->p[def_idx].units[i] = 0;
    } else if (sb > 0) {
        for(int i=0; i<4; i++) {
            game->p[def_idx].units[i] -= (game->p[def_idx].units[i] * sa) / sb;
        }
    }

    // Straty atakujacego (odwet obroncy)
    long sa_ret = 0, sb_ret = 0;
    for(int i=0; i<3; i++) sa_ret += game->p[def_idx].units[i] * ATK_VAL[i];
    for(int i=0; i<3; i++) sb_ret += game->p[atk_idx].att_units[i] * DEF_VAL[i];

    if (sa_ret >= sb_ret && sb_ret > 0) {
        for(int i=0; i<4; i++) game->p[atk_idx].att_units[i] = 0;
    } else if (sb_ret > 0) {
        for(int i=0; i<4; i++) {
            game->p[atk_idx].att_units[i] -= (game->p[atk_idx].att_units[i] * sa_ret) / sb_ret;
        }
    }

    // Powrot ocalalych
    for(int i=0; i<4; i++) {
        game->p[atk_idx].units[i] += game->p[atk_idx].att_units[i];
        game->p[atk_idx].att_units[i] = 0;
    }
}

int main() {
    msg_id = msgget(KLUCZ, 0666 | IPC_CREAT);
    shm_id = shmget(SHM_KLUCZ, sizeof(GameState), 0666 | IPC_CREAT);
    game = shmat(shm_id, NULL, 0);
    sem_id = semget(SEM_KLUCZ, 1, 0666 | IPC_CREAT);
    
    semctl(sem_id, 0, SETVAL, 1); // Inicjalizacja semafora [cite: 255]
    for(int i=0; i<2; i++) {
        game->p[i].active = 0;
        game->p[i].resources = 300;
        game->p[i].training_type = -1;
    }

    if (fork() == 0) { // PROCES LOGIKI
        while(1) {
            sleep(1);
            sem_lock();
            for(int i=0; i<2; i++) {
                if(!game->p[i].active) continue;
                // Surowce
                game->p[i].resources += 50 + (game->p[i].units[3] * 5);
                // Trening
                if(game->p[i].training_type != -1) {
                    game->p[i].training_time_left--;
                    if(game->p[i].training_time_left <= 0) {
                        game->p[i].units[game->p[i].training_type]++;
                        game->p[i].training_count--;
                        if(game->p[i].training_count > 0)
                            game->p[i].training_time_left = TIMES[game->p[i].training_type];
                        else
                            game->p[i].training_type = -1;
                    }
                }
                // Atak
                if(game->p[i].is_attacking) {
                    game->p[i].att_time_left--;
                    if(game->p[i].att_time_left <= 0) {
                        oblicz_walke(i);
                        game->p[i].is_attacking = 0;
                    }
                }
            }
            sem_unlock();
        }
    } else { // PROCES KOMUNIKACJI
        MsgBuf m;
        while(msgrcv(msg_id, &m, sizeof(m)-sizeof(long), -20, 0) > 0) {
            sem_lock();
            if(m.wybor == MSG_LOGOWANIE) {
                int id = (game->p[0].active == 0) ? 0 : (game->p[1].active == 0 ? 1 : -1);
                if(id != -1) game->p[id].active = 1;
                m.mtype = m.id_gracza; // Odpowiedz do konkretnego PID
                m.id_gracza = id;
                msgsnd(msg_id, &m, sizeof(m)-sizeof(long), 0);
            } else if(m.wybor == MSG_BUDUJ) {
                int p = m.id_gracza;
                int type = m.jednostki[0];
                int count = m.jednostki[1];
                if(game->p[p].training_type == -1 && game->p[p].resources >= COSTS[type]*count) {
                    game->p[p].resources -= COSTS[type]*count;
                    game->p[p].training_type = type;
                    game->p[p].training_count = count;
                    game->p[p].training_time_left = TIMES[type];
                }
            } else if(m.wybor == MSG_ATAK) {
                int p = m.id_gracza;
                if(!game->p[p].is_attacking) {
                    game->p[p].is_attacking = 1;
                    game->p[p].att_time_left = 5;
                    for(int i=0; i<3; i++) {
                        int num = (m.jednostki[i] > game->p[p].units[i]) ? game->p[p].units[i] : m.jednostki[i];
                        game->p[p].att_units[i] = num;
                        game->p[p].units[i] -= num;
                    }
                }
            } else if(m.wybor == MSG_STAN) {
                int p = m.id_gracza;
                m.mtype = m.id_gracza;
                m.zasoby = game->p[p].resources;
                m.punkty = game->p[p].points;
                for(int i=0; i<4; i++) m.jednostki[i] = game->p[p].units[i];
                msgsnd(msg_id, &m, sizeof(m)-sizeof(long), 0);
            }
            sem_unlock();
        }
    }
    return 0;
}