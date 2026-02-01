#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "common.h"

int shm_id, sem_id, msg_id;
GameState *game;

struct sembuf p_op = {0, -1, 0};
struct sembuf v_op = {0, 1, 0};

void my_write(const char* str) {
    write(1, str, strlen(str));
}

void clean_exit(int sig) {
    shmdt(game);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);
    exit(0);
}

void init_ipc() {
    key_t key = ftok(".", KEY_ID);
    shm_id = shmget(key, sizeof(GameState), IPC_CREAT | 0666);
    game = (GameState*)shmat(shm_id, NULL, 0);
    memset(game, 0, sizeof(GameState));
    
    sem_id = semget(key, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1);
    
    msg_id = msgget(key, IPC_CREAT | 0666);
    
    game->players[0].gold = 300;
    game->players[1].gold = 300;
    game->players[0].id = 0;
    game->players[1].id = 1;
}

double get_attack(int type) {
    if (type == UNIT_LIGHT) return 1.0;
    if (type == UNIT_HEAVY) return 1.5;
    if (type == UNIT_CAVALRY) return 3.5;
    return 0.0;
}

double get_defense(int type) {
    if (type == UNIT_LIGHT) return 1.2;
    if (type == UNIT_HEAVY) return 3.0;
    if (type == UNIT_CAVALRY) return 1.2;
    return 0.0;
}

int get_cost(int type) {
    if (type == UNIT_LIGHT) return 100;
    if (type == UNIT_HEAVY) return 250;
    if (type == UNIT_CAVALRY) return 550;
    if (type == UNIT_WORKER) return 150;
    return 0;
}

int get_time(int type) {
    if (type == UNIT_LIGHT) return 2;
    if (type == UNIT_HEAVY) return 3;
    if (type == UNIT_CAVALRY) return 5;
    if (type == UNIT_WORKER) return 2;
    return 0;
}

void resolve_combat(int attacker_id, int u_light, int u_heavy, int u_cav) {
    int defender_id = (attacker_id + 1) % 2;
    PlayerData *att = &game->players[attacker_id];
    PlayerData *def = &game->players[defender_id];

    double sa = u_light * get_attack(UNIT_LIGHT) + u_heavy * get_attack(UNIT_HEAVY) + u_cav * get_attack(UNIT_CAVALRY);
    double sb = def->units[UNIT_LIGHT] * get_defense(UNIT_LIGHT) + 
                def->units[UNIT_HEAVY] * get_defense(UNIT_HEAVY) + 
                def->units[UNIT_CAVALRY] * get_defense(UNIT_CAVALRY); // Workers don't defend

    char buf[256];
    int won = 0;

    if (sa > sb) {
        won = 1;
        att->wins++;
        memset(def->units, 0, sizeof(def->units)); 
        def->worker_count = 0; 
    } else {
        if (sb > 0) {
            for (int i = 0; i < 3; i++) {
                int loss = floor(def->units[i] * (sa / sb));
                def->units[i] -= loss;
                if (def->units[i] < 0) def->units[i] = 0;
            }
        }
    }

    double sb_counter = def->units[UNIT_LIGHT] * get_attack(UNIT_LIGHT) + 
                        def->units[UNIT_HEAVY] * get_attack(UNIT_HEAVY) + 
                        def->units[UNIT_CAVALRY] * get_attack(UNIT_CAVALRY);
    
    double sa_defense = (att->units[UNIT_LIGHT] - u_light) * get_defense(UNIT_LIGHT) +
                        (att->units[UNIT_HEAVY] - u_heavy) * get_defense(UNIT_HEAVY) +
                        (att->units[UNIT_CAVALRY] - u_cav) * get_defense(UNIT_CAVALRY); 

    sa_defense += u_light * 1.2 + u_heavy * 3.0 + u_cav * 1.2;

    if (sb_counter > 0 && sa_defense > 0) {
       
    }

    Message msg;
    msg.mtype = game->client_pids[attacker_id];
    msg.cmd = MSG_RESULT;
    if (won) sprintf(msg.text, "Atak udany! Wygrales bitwe.");
    else sprintf(msg.text, "Atak odparty przez wroga.");
    msgsnd(msg_id, &msg, sizeof(Message) - sizeof(long), 0);

    msg.mtype = game->client_pids[defender_id];
    if (won) sprintf(msg.text, "Twoja baza zostala zniszczona!");
    else sprintf(msg.text, "Odparles atak wroga.");
    msgsnd(msg_id, &msg, sizeof(Message) - sizeof(long), 0);
}

void logic_loop() {
    while (1) {
        semop(sem_id, &p_op, 1);
        
        for (int i = 0; i < 2; i++) {
            if (game->client_pids[i] == 0) continue;

            game->players[i].gold += 50 + (game->players[i].worker_count * 5);

            int head = game->queue_head[i];
            int tail = game->queue_tail[i];
            
            if (head != tail) {
                game->build_queue[i][head].time_left--;
                if (game->build_queue[i][head].time_left <= 0) {
                    int type = game->build_queue[i][head].type;
                    int count = game->build_queue[i][head].count;
                    if (type == UNIT_WORKER) game->players[i].worker_count += count;
                    else game->players[i].units[type] += count;
                    
                    game->queue_head[i] = (head + 1) % MAX_QUEUE;
                }
            }

            if (game->players[i].wins >= 5) {
                Message msg;
                msg.cmd = MSG_GAME_OVER;
                msg.mtype = game->client_pids[i];
                sprintf(msg.text, "GRATULACJE! WYGRALES GRE!");
                msgsnd(msg_id, &msg, sizeof(Message) - sizeof(long), 0);

                msg.mtype = game->client_pids[(i + 1) % 2];
                sprintf(msg.text, "PRZEGRALES GRE. SPROBUJ PONOWNIE.");
                msgsnd(msg_id, &msg, sizeof(Message) - sizeof(long), 0);
                
                semop(sem_id, &v_op, 1);
                sleep(1);
                kill(getppid(), SIGINT);
                exit(0);
            }
        }
        
        semop(sem_id, &v_op, 1);
        
        for(int i=0; i<2; i++) {
            if (game->client_pids[i] != 0) {
                Message update;
                update.mtype = game->client_pids[i];
                update.cmd = MSG_STATE;
                update.args[0] = game->players[i].gold;
                update.args[1] = game->players[i].units[0]; 
                update.args[2] = game->players[i].units[1];
                update.args[3] = game->players[i].units[2];
                msgsnd(msg_id, &update, sizeof(Message) - sizeof(long), 0);
            }
        }
        sleep(1);
    }
}

int main() {
    signal(SIGINT, clean_exit);
    init_ipc();
    
    if (fork() == 0) {
        logic_loop();
        exit(0);
    }

    my_write("Serwer uruchomiony. Oczekiwanie na graczy...\n");

    Message msg;
    while (1) {
        msgrcv(msg_id, &msg, sizeof(Message) - sizeof(long), 1, 0);
        
        semop(sem_id, &p_op, 1);
        
        if (msg.cmd == CMD_LOGIN) {
            if (game->connected_clients < 2) {
                int pid = msg.src_id;
                int idx = game->connected_clients;
                game->client_pids[idx] = pid;
                game->connected_clients++;
                
                Message reply;
                reply.mtype = pid;
                reply.cmd = CMD_LOGIN;
                reply.args[0] = idx; 
                msgsnd(msg_id, &reply, sizeof(Message) - sizeof(long), 0);
            }
        } else if (msg.cmd == CMD_BUILD) {
            int p_idx = -1;
            if (msg.src_id == game->client_pids[0]) p_idx = 0;
            else if (msg.src_id == game->client_pids[1]) p_idx = 1;

            if (p_idx != -1) {
                int type = msg.args[0];
                int count = msg.args[1];
                int cost = get_cost(type) * count;
                
                if (game->players[p_idx].gold >= cost) {
                    int tail = game->queue_tail[p_idx];
                    int next_tail = (tail + 1) % MAX_QUEUE;
                    if (next_tail != game->queue_head[p_idx]) {
                        game->players[p_idx].gold -= cost;
                        game->build_queue[p_idx][tail].type = type;
                        game->build_queue[p_idx][tail].count = count;
                        game->build_queue[p_idx][tail].time_left = get_time(type); // simplified: full time per batch
                        game->queue_tail[p_idx] = next_tail;
                    }
                }
            }
        } else if (msg.cmd == CMD_ATTACK) {
             int p_idx = -1;
            if (msg.src_id == game->client_pids[0]) p_idx = 0;
            else if (msg.src_id == game->client_pids[1]) p_idx = 1;
            
            if (p_idx != -1) {
                 int ul = msg.args[0];
                 int uh = msg.args[1];
                 int uc = msg.args[2];
                 
                 if (game->players[p_idx].units[UNIT_LIGHT] >= ul &&
                     game->players[p_idx].units[UNIT_HEAVY] >= uh &&
                     game->players[p_idx].units[UNIT_CAVALRY] >= uc) {
                         
                     resolve_combat(p_idx, ul, uh, uc);
                 }
            }
        }

        semop(sem_id, &v_op, 1);
    }
}