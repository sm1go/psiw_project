#include "common.h"

int shm_id, sem_id, msg_id;
GameState *game;

void sem_lock() {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) perror("sem_lock");
    }
}

void sem_unlock() {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) perror("sem_unlock");
    }
}

void process_build(int player_id, int unit_type, int count) {
    if (fork() == 0) {
        for (int i = 0; i < count; i++) {
            sleep(TIME_VAL[unit_type]);
            sem_lock();
            game->players[player_id].units[unit_type]++;
            sem_unlock();
        }
        exit(0);
    }
}

void calculate_battle_result(int att_id, int def_id, int *att_army) {
    sem_lock();

    double SA = 0;
    for(int i=0; i<4; i++) SA += att_army[i] * ATTACK_VAL[i];

    double SB = 0;
    for(int i=0; i<4; i++) SB += game->players[def_id].units[i] * DEFENSE_VAL[i];

    char report[MAX_TEXT];
    sprintf(report, "Bitwa! Ty (Sila: %.1f) vs Wrog (Obrona: %.1f). ", SA, SB);

    if (SA > SB) {
        strcat(report, "ZWYCIESTWO! Baza wroga zniszczona.");
        game->players[att_id].score++;

        for(int i=0; i<4; i++) game->players[def_id].units[i] = 0;
    } else {
        strcat(report, "PORAZKA. Atak odparty.");

        for(int i=0; i<4; i++) {
            int X = game->players[def_id].units[i];
            if(X > 0 && SB > 0) {
                int lost = (int)floor(X * SA / SB);
                game->players[def_id].units[i] -= lost;
                if(game->players[def_id].units[i] < 0) game->players[def_id].units[i] = 0;
            }
        }

        double SB_attack = 0;
        for(int i=0; i<4; i++) SB_attack += game->players[def_id].units[i] * ATTACK_VAL[i];

        double SA_defense = 0;
        for(int i=0; i<4; i++) SA_defense += att_army[i] * DEFENSE_VAL[i];

        if (SA_defense > 0) {
             for(int i=0; i<4; i++) {
                int X = att_army[i];
                if (X > 0) {
                    int lost = (int)floor(X * SB_attack / SA_defense);
                    att_army[i] -= lost;
                    if(att_army[i] < 0) att_army[i] = 0;
                }
             }
        }
    }

    for(int i=0; i<4; i++) {
        game->players[att_id].units[i] += att_army[i];
    }

    sem_unlock();

    GameMsg msg;
    msg.mtype = 20 + att_id;
    msg.cmd_type = MSG_RESULT;
    strcpy(msg.text, report);
    msgsnd(msg_id, &msg, sizeof(GameMsg)-sizeof(long), IPC_NOWAIT);
}

void process_attack(int att_id, int *army_comp) {
    if (fork() == 0) {
        sleep(5);
        int def_id = (att_id == 0) ? 1 : 0;
        calculate_battle_result(att_id, def_id, army_comp);
        exit(0);
    }
}

void process_push_updates() {
    if (fork() == 0) {
        while(1) {
            usleep(100000);
            sem_lock();
            
            if (!game->players[0].is_active || !game->players[1].is_active) {
                sem_unlock();
                continue;
            }

            for(int i=0; i<2; i++) {
                GameMsg update;
                update.mtype = 10 + i;
                update.cmd_type = MSG_UPDATE;
                update.gold_now = (int)game->players[i].gold;
                memcpy(update.units_now, game->players[i].units, sizeof(int)*4);
                update.score_me = game->players[i].score;
                update.score_enemy = game->players[(i==0)?1:0].score;
                msgsnd(msg_id, &update, sizeof(GameMsg)-sizeof(long), IPC_NOWAIT);
            }
            sem_unlock();
        }
        exit(0);
    }
}

void process_resources() {
    if (fork() == 0) {
        while(1) {
            usleep(100000);
            sem_lock();

            if (!game->players[0].is_active || !game->players[1].is_active) {
                sem_unlock();
                continue;
            }

            for(int i=0; i<2; i++) {
                double base_gain = 50.0 / 10.0;
                double worker_gain = (game->players[i].units[WORKER] * 5.0) / 10.0;
                game->players[i].gold += (base_gain + worker_gain);
            }
            sem_unlock();
        }
        exit(0);
    }
}

void cleanup(int sig) {
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    printf("\nSerwer zamkniety.\n");
    exit(0);
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGCHLD, SIG_IGN);

    key_t key = ftok(".", PROJECT_ID);

    shm_id = shmget(key, sizeof(GameState), IPC_CREAT | 0666);
    game = (GameState*)shmat(shm_id, NULL, 0);
    memset(game, 0, sizeof(GameState));

    game->players[0].gold = 300;
    game->players[1].gold = 300;

    sem_id = semget(key, 1, IPC_CREAT | 0666);
    union semun arg;
    arg.val = 1;
    semctl(sem_id, 0, SETVAL, arg);

    msg_id = msgget(key, IPC_CREAT | 0666);

    process_resources();
    process_push_updates();

    printf("Serwer gotowy. Czekam na graczy...\n");

    GameMsg msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(GameMsg)-sizeof(long), 0, 0) == -1) {
             if(errno != EINTR) perror("msgrcv");
             continue;
        }

        if (msg.cmd_type == MSG_UPDATE || msg.cmd_type == MSG_RESULT) continue;

        int pid = msg.source_id;

        if (msg.cmd_type == MSG_LOGIN) {
            printf("Gracz %d dolaczyl.\n", pid + 1);
            sem_lock();
            game->players[pid].is_active = 1;
            game->players[pid].pid = msg.count;
            sem_unlock();
        }
        else if (msg.cmd_type == MSG_BUILD) {
            int cost = COST[msg.unit_type] * msg.count;
            sem_lock();
            if (game->players[pid].gold >= cost) {
                game->players[pid].gold -= cost;
                sem_unlock();
                printf("G%d: Buduje %d x Typ %d\n", pid+1, msg.count, msg.unit_type);
                process_build(pid, msg.unit_type, msg.count);
            } else {
                sem_unlock();
            }
        }
        else if (msg.cmd_type == MSG_ATTACK) {
            sem_lock();
            int possible = 1;
            for(int i=0; i<4; i++) {
                if (game->players[pid].units[i] < msg.attack_army[i]) possible = 0;
            }

            if (possible && msg.unit_type != WORKER) {
                for(int i=0; i<4; i++) game->players[pid].units[i] -= msg.attack_army[i];
                sem_unlock();

                printf("G%d: Atakuje!\n", pid+1);
                process_attack(pid, msg.attack_army);
            } else {
                sem_unlock();
            }
        }
    }
    return 0;
}