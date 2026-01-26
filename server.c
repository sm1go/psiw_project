#include "common.h"

int shmid, semid, msgid;
struct GameState *game;

struct UnitStats stats[4] = {
    {100, 1.0, 1.2, 2},
    {250, 1.5, 3.0, 3},
    {550, 3.5, 1.2, 5},
    {150, 0.0, 0.0, 2}
};

void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

void clean_exit(int sig) {
    shmdt(game);
    shmctl(shmid, IPC_RMID, 0);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, 0);
    exit(0);
}

void process_production(struct PlayerState *p) {
    for(int i=0; i<4; i++) {
        if(p->production_queue[i] > 0) {
            p->production_timer[i]--;
            if(p->production_timer[i] <= 0) {
                p->units[i]++;
                p->production_queue[i]--;
                if(p->production_queue[i] > 0) {
                    p->production_timer[i] = stats[i].build_time;
                } else {
                    p->production_timer[i] = 0;
                }
            }
        }
    }
    int workers = p->units[3];
    p->resources += 50 + (workers * 5);
}

void timer_handler(int sig) {
    sem_lock(semid);
    
    process_production(&game->p1);
    process_production(&game->p2);

    struct UpdateMsg msg1, msg2;
    msg1.mtype = 10; 
    msg1.self = game->p1;
    msg1.enemy = game->p2; 
    msg1.enemy.resources = 0; 
    for(int i=0; i<100; i++) msg1.message[i] = game->last_event_msg[i];
    
    msg2.mtype = 20;
    msg2.self = game->p2;
    msg2.enemy = game->p1;
    msg2.enemy.resources = 0;
    for(int i=0; i<100; i++) msg2.message[i] = game->last_event_msg[i];

    if(my_strlen(game->last_event_msg) > 0) {
        game->last_event_msg[0] = '\0';
    }

    msgsnd(msgid, &msg1, sizeof(struct UpdateMsg) - sizeof(long), IPC_NOWAIT);
    msgsnd(msgid, &msg2, sizeof(struct UpdateMsg) - sizeof(long), IPC_NOWAIT);

    sem_unlock(semid);
    alarm(1);
}

void handle_attack(int attacker_id, int u0, int u1, int u2) {
    struct PlayerState *att, *def;
    if(attacker_id == 1) { att = &game->p1; def = &game->p2; }
    else { att = &game->p2; def = &game->p1; }

    if(att->units[0] < u0 || att->units[1] < u1 || att->units[2] < u2) {
        return; 
    }

    float attack_power = u0 * stats[0].attack + u1 * stats[1].attack + u2 * stats[2].attack;
    float defense_power = def->units[0] * stats[0].defense + 
                          def->units[1] * stats[1].defense + 
                          def->units[2] * stats[2].defense +
                          def->units[3] * stats[3].defense; 

    if(attack_power > defense_power) {
        att->victory_points++;
        for(int i=0; i<4; i++) def->units[i] = 0;
        
        char *msg = (attacker_id == 1) ? "Gracz 1 wygral bitwe!" : "Gracz 2 wygral bitwe!";
        int k=0; while(msg[k]) { game->last_event_msg[k] = msg[k]; k++; } game->last_event_msg[k] = 0;

    } else {
        for(int i=0; i<4; i++) {
            if(def->units[i] > 0) {
                int lost = (int)(def->units[i] * (attack_power / defense_power));
                def->units[i] -= lost;
                if(def->units[i] < 0) def->units[i] = 0;
            }
        }
        char *msg = "Atak odparty!";
        int k=0; while(msg[k]) { game->last_event_msg[k] = msg[k]; k++; } game->last_event_msg[k] = 0;
    }

    float counter_att = def->units[0] * stats[0].attack + def->units[1] * stats[1].attack + def->units[2] * stats[2].attack; 
    float counter_def = u0 * stats[0].defense + u1 * stats[1].defense + u2 * stats[2].defense;

    if(counter_att > counter_def) {
         att->units[0] -= u0;
         att->units[1] -= u1;
         att->units[2] -= u2;
    } else {
        int lost0 = (int)(u0 * (counter_att / counter_def));
        int lost1 = (int)(u1 * (counter_att / counter_def));
        int lost2 = (int)(u2 * (counter_att / counter_def));
        
        att->units[0] -= lost0;
        att->units[1] -= lost1;
        att->units[2] -= lost2;
    }

    if(att->victory_points >= 5) {
        char *win = (attacker_id == 1) ? "KONIEC: WYGRAL GRACZ 1" : "KONIEC: WYGRAL GRACZ 2";
        int k=0; while(win[k]) { game->last_event_msg[k] = win[k]; k++; } game->last_event_msg[k] = 0;
    }
}

int main() {
    signal(SIGINT, clean_exit);

    shmid = shmget(KEY_SHM, sizeof(struct GameState), IPC_CREAT | 0666);
    game = (struct GameState*)shmat(shmid, NULL, 0);

    msgid = msgget(KEY_MSG, IPC_CREAT | 0666);
    semid = semget(KEY_SEM, 1, IPC_CREAT | 0666);

    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    game->connected_clients = 0;
    game->p1.id = 1; game->p1.resources = 300; game->p1.victory_points = 0;
    game->p2.id = 2; game->p2.resources = 300; game->p2.victory_points = 0;
    for(int i=0;i<4;i++) {
        game->p1.units[i]=0; game->p1.production_queue[i]=0;
        game->p2.units[i]=0; game->p2.production_queue[i]=0;
    }

    if(fork() == 0) {
        signal(SIGALRM, timer_handler);
        alarm(1);
        while(1) {
            pause();
        }
    }

    struct MsgBuf req;
    while(1) {
        if(msgrcv(msgid, &req, sizeof(struct MsgBuf) - sizeof(long), 0, 0) == -1) {
            if(game->p1.victory_points >= 5 || game->p2.victory_points >= 5) break;
            continue;
        }

        sem_lock(semid);

        if(req.mtype == MSG_TYPE_LOGIN) {
            game->connected_clients++;
        }
        else if(req.command == 1) { 
            struct PlayerState *p = (req.player_id == 1) ? &game->p1 : &game->p2;
            int cost = stats[req.unit_type].cost * req.count;
            if(p->resources >= cost) {
                p->resources -= cost;
                if(p->production_queue[req.unit_type] == 0) {
                    p->production_timer[req.unit_type] = stats[req.unit_type].build_time;
                }
                p->production_queue[req.unit_type] += req.count;
            }
        }
        else if(req.command == 2) { 
            handle_attack(req.player_id, req.units_to_attack[0], req.units_to_attack[1], req.units_to_attack[2]);
        }

        sem_unlock(semid);
    }

    return 0;
}