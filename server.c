#include "common.h"

int msgid, shmid, semid;
GameState *game;

void cleanup_server(int sig) {
    msgctl(msgid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    exit(0);
}

int main() {
    signal(SIGINT, cleanup_server);
    signal(SIGCHLD, SIG_IGN);

    msgid = msgget(KEY, IPC_CREAT | 0600);
    shmid = shmget(KEY, sizeof(GameState), IPC_CREAT | 0600);
    semid = semget(KEY, 1, IPC_CREAT | 0600);

    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    game = (GameState*)shmat(shmid, NULL, 0);
    memset(game, 0, sizeof(GameState));
    game->resources[0] = 300; // Startowe zasoby
    game->resources[1] = 300;

    // Proces produkcji surowców
    if (fork() == 0) {
        while(1) {
            sleep(1);
            lock_sync(semid);
            if (game->game_active) {
                for(int i=0; i<2; i++) {
                    game->resources[i] += 50 + (game->army[i][WORKER] * 5);
                }
            }
            unlock_sync(semid);
        }
    }

    int players_count = 0;
    GameMsg msg;

    while(1) {
        msgrcv(msgid, &msg, sizeof(GameMsg)-sizeof(long), 1, 0); // Odbiór na typie 1

        if (msg.action_type == M_LOGIN) {
            lock_sync(semid);
            msg.mtype = msg.player_id; // Powrót do PID klienta
            if (players_count < 2) {
                msg.player_id = ++players_count;
                if (players_count == 2) game->game_active = 1;
            } else {
                msg.player_id = -1;
            }
            msgsnd(msgid, &msg, sizeof(GameMsg)-sizeof(long), 0);
            unlock_sync(semid);
        } 
        else if (msg.action_type == M_STATE) {
            int p = msg.player_id - 1;
            lock_sync(semid);
            msg.mtype = msg.player_id;
            msg.resources = game->resources[p];
            msg.points = game->points[p];
            msg.status = game->game_active;
            for(int i=0; i<4; i++) msg.army[i] = game->army[p][i];
            
            // Sprawdzenie przegranej (przeciwnik ma 5 pkt)
            int enemy = (p == 0) ? 1 : 0;
            if (game->points[enemy] >= WIN_POINTS) msg.points = -1; 

            msgsnd(msgid, &msg, sizeof(GameMsg)-sizeof(long), 0);
            unlock_sync(semid);
        }
        else if (msg.action_type == M_BUILD) {
            int p = msg.player_id - 1;
            int cost = UNITS[msg.unit_idx].price * msg.count;
            lock_sync(semid);
            if (game->resources[p] >= cost) {
                game->resources[p] -= cost;
                unlock_sync(semid);
                if (fork() == 0) { // Proces treningu
                    for(int i=0; i<msg.count; i++) {
                        sleep(UNITS[msg.unit_idx].build_time);
                        lock_sync(semid);
                        game->army[p][msg.unit_idx]++;
                        unlock_sync(semid);
                    }
                    exit(0);
                }
            } else unlock_sync(semid);
        }
        else if (msg.action_type == M_ATTACK) {
            int p = msg.player_id - 1;
            int enemy = (p == 0) ? 1 : 0;
            lock_sync(semid);
            
            // Weryfikacja dostępności wojska
            int can_attack = 1;
            for(int i=0; i<3; i++) if(game->army[p][i] < msg.army[i]) can_attack = 0;

            if(can_attack) {
                for(int i=0; i<3; i++) game->army[p][i] -= msg.army[i];
                unlock_sync(semid);
                if(fork() == 0) {
                    int attackers[4];
                    for(int i=0; i<4; i++) attackers[i] = msg.army[i];
                    sleep(5); // Czas trwania ataku
                    
                    lock_sync(semid);
                    float SA = 0, SB = 0, ESA = 0, ESB = 0;
                    for(int i=0; i<3; i++) {
                        SA += attackers[i] * UNITS[i].attack;
                        SB += game->army[enemy][i] * UNITS[i].defense;
                        ESA += game->army[enemy][i] * UNITS[i].attack;
                        ESB += attackers[i] * UNITS[i].defense;
                    }
                    
                    // Straty obrońcy i punktacja
                    if (SA > SB) {
                        game->points[p]++;
                        for(int i=0; i<3; i++) game->army[enemy][i] = 0;
                    } else {
                        float ratio = (SB > 0) ? SA/SB : 0;
                        for(int i=0; i<3; i++) game->army[enemy][i] -= (int)(game->army[enemy][i] * ratio);
                    }
                    // Straty atakującego (kontratak)
                    if (ESA > ESB) {
                        for(int i=0; i<3; i++) attackers[i] = 0;
                    } else {
                        float ratio = (ESB > 0) ? ESA/ESB : 0;
                        for(int i=0; i<3; i++) attackers[i] -= (int)(attackers[i] * ratio);
                    }
                    for(int i=0; i<3; i++) game->army[p][i] += attackers[i];
                    unlock_sync(semid);
                    exit(0);
                }
            } else unlock_sync(semid);
        }
    }
}