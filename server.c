#include "common.h"

int shm_id, sem_id, msg_id;
GameState *game;

void cleanup(int sig) {
    shmdt(game);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);
    exit(0);
}

void semaphore_op(int op) {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = op;
    sb.sem_flg = 0;
    semop(sem_id, &sb, 1);
}

void send_end_game(int winner_id) {
    Message msg;
    msg.cmd = CMD_RESULT;
    game->game_over = 1;

    for(int i=0; i<2; i++) {
        msg.type = 10 + i;
        if (i == winner_id) {
            sprintf(msg.text, "GRATULACJE! WYGRALES GRE!");
        } else {
            sprintf(msg.text, "PORAZKA! PRZECIWNIK ZDOBYL 5 PUNKTOW.");
        }
        msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
    }
}

void send_update() {
    Message msg;
    msg.cmd = CMD_UPDATE;
    
    for(int i=0; i<2; i++) {
        msg.type = 10 + i;
        sprintf(msg.text, "ZASOBY: %d | ROB: %d | WIN: %d\nARMIA: L:%d C:%d J:%d\nPRODUKCJA: %d zadan", 
            game->players[i].resources, 
            game->players[i].units[UNIT_WORKER], 
            game->players[i].wins,
            game->players[i].units[UNIT_LIGHT],
            game->players[i].units[UNIT_HEAVY],
            game->players[i].units[UNIT_CAVALRY],
            game->players[i].q_size);
        msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), IPC_NOWAIT);
    }
}

void resolve_attack(int attacker_id, int units_sent[3]) {
    int defender_id = (attacker_id == 0) ? 1 : 0;
    
    double attack_power = 
        units_sent[UNIT_LIGHT] * UNITS[UNIT_LIGHT].attack +
        units_sent[UNIT_HEAVY] * UNITS[UNIT_HEAVY].attack +
        units_sent[UNIT_CAVALRY] * UNITS[UNIT_CAVALRY].attack;

    double defense_power = 
        game->players[defender_id].units[UNIT_LIGHT] * UNITS[UNIT_LIGHT].defense +
        game->players[defender_id].units[UNIT_HEAVY] * UNITS[UNIT_HEAVY].defense +
        game->players[defender_id].units[UNIT_CAVALRY] * UNITS[UNIT_CAVALRY].defense;

    if (defense_power == 0) defense_power = 0.1;

    if (attack_power > defense_power) {
        game->players[attacker_id].wins++;
        for(int u=0; u<3; u++) game->players[defender_id].units[u] = 0;

        if (game->players[attacker_id].wins >= 5) {
            send_end_game(attacker_id);
        }

    } else {
        double ratio_def = attack_power / defense_power;
        for(int u=0; u<3; u++) {
            int lost = floor(game->players[defender_id].units[u] * ratio_def);
            game->players[defender_id].units[u] -= lost;
        }
    }

    double defender_attack = 
        game->players[defender_id].units[UNIT_LIGHT] * UNITS[UNIT_LIGHT].attack +
        game->players[defender_id].units[UNIT_HEAVY] * UNITS[UNIT_HEAVY].attack +
        game->players[defender_id].units[UNIT_CAVALRY] * UNITS[UNIT_CAVALRY].attack;
        
    double attacker_defense = 
        units_sent[UNIT_LIGHT] * UNITS[UNIT_LIGHT].defense +
        units_sent[UNIT_HEAVY] * UNITS[UNIT_HEAVY].defense +
        units_sent[UNIT_CAVALRY] * UNITS[UNIT_CAVALRY].defense;

    if (attacker_defense == 0) attacker_defense = 0.1;

    double ratio_atk = defender_attack / attacker_defense;
    if (ratio_atk > 1.0) ratio_atk = 1.0;

    for(int u=0; u<3; u++) {
        int lost = floor(units_sent[u] * ratio_atk);
        int returning = units_sent[u] - lost;
        game->players[attacker_id].units[u] += returning;
    }
}

int main() {
    signal(SIGINT, cleanup);

    shm_id = shmget(KEY_SHM, sizeof(GameState), IPC_CREAT | 0666);
    game = (GameState*)shmat(shm_id, NULL, 0);
    
    sem_id = semget(KEY_SEM, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1);

    msg_id = msgget(KEY_MSG, IPC_CREAT | 0666);

    memset(game, 0, sizeof(GameState));
    game->players[0].resources = 300;
    game->players[1].resources = 300;

    if (fork() == 0) {
        while(1) {
            sleep(1);
            semaphore_op(-1);
            
            if (game->game_over) {
                semaphore_op(1);
                exit(0);
            }

            for(int i=0; i<2; i++) {
                game->players[i].resources += 50 + (game->players[i].units[UNIT_WORKER] * 5);
                
                if (game->players[i].q_size > 0) {
                    game->players[i].production_queue[0].time_left--;
                    if (game->players[i].production_queue[0].time_left <= 0) {
                        int type = game->players[i].production_queue[0].type;
                        int count = game->players[i].production_queue[0].count;
                        game->players[i].units[type] += count;
                        
                        for(int k=0; k < game->players[i].q_size - 1; k++) {
                            game->players[i].production_queue[k] = game->players[i].production_queue[k+1];
                        }
                        game->players[i].q_size--;
                    }
                }
            }
            send_update();
            semaphore_op(1);
        }
    } else {
        Message msg;
        while(1) {
            msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), 1, 0);
            
            semaphore_op(-1);

            if (game->game_over) {
                semaphore_op(1);
                break;
            }

            if (msg.cmd == CMD_LOGIN) {
                if (game->connected_clients < 2) {
                    msg.type = msg.source_id; 
                    msg.args[0] = game->connected_clients;
                    game->connected_clients++;
                    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                }
            } else if (msg.cmd == CMD_TRAIN) {
                int p_idx = msg.args[0];
                int type = msg.args[1];
                int count = msg.args[2];
                int cost = UNITS[type].cost * count;
                
                if (game->players[p_idx].resources >= cost && game->players[p_idx].q_size < 10) {
                    game->players[p_idx].resources -= cost;
                    int q = game->players[p_idx].q_size;
                    game->players[p_idx].production_queue[q].type = type;
                    game->players[p_idx].production_queue[q].count = count;
                    game->players[p_idx].production_queue[q].time_left = UNITS[type].time;
                    game->players[p_idx].q_size++;
                }
            } else if (msg.cmd == CMD_ATTACK) {
                int p_idx = msg.args[0];
                int l = msg.args[1];
                int h = msg.args[2];
                int c = msg.args[3];
                
                if (game->players[p_idx].units[UNIT_LIGHT] >= l &&
                    game->players[p_idx].units[UNIT_HEAVY] >= h &&
                    game->players[p_idx].units[UNIT_CAVALRY] >= c) {
                        
                    game->players[p_idx].units[UNIT_LIGHT] -= l;
                    game->players[p_idx].units[UNIT_HEAVY] -= h;
                    game->players[p_idx].units[UNIT_CAVALRY] -= c;
                    
                    int sent[3] = {l, h, c};
                    sleep(5); 
                    resolve_attack(p_idx, sent);
                }
            }
            semaphore_op(1);
        }
    }
    return 0;
}