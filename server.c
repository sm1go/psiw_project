#include "common.h"

int shmid, semid, msgid;
struct GameState *gs;

void my_memcpy(void *dest, void *src, int n) {
    char *csrc = (char *)src;
    char *cdest = (char *)dest;
    int i;
    for (i=0; i<n; i++) cdest[i] = csrc[i];
}

void init_stats(struct UnitStats *s, int c, int a, int d, int t) {
    s->cost = c; s->attack = a; s->defense = d; s->time = t;
}

void get_stats(int type, struct UnitStats *s) {
    if (type == U_LIGHT) init_stats(s, 100, 10, 12, 2);
    else if (type == U_HEAVY) init_stats(s, 250, 15, 30, 3);
    else if (type == U_CAV) init_stats(s, 550, 35, 12, 5);
    else if (type == U_WORK) init_stats(s, 150, 0, 0, 2);
}

void sem_lock() {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void sem_unlock() {
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void handle_training(struct Player *p, long current_tick) {
    if (p->training.active) {
        if (current_tick >= p->training.next_tick) {
            p->units[p->training.unit_type]++;
            p->training.count--;
            if (p->training.count > 0) {
                struct UnitStats s;
                get_stats(p->training.unit_type, &s);
                p->training.next_tick = current_tick + s.time;
            } else {
                p->training.active = 0;
            }
        }
    }
}

void resolve_battle() {
    struct Player *att, *def;
    if (gs->battle.attacker_id == 1) { att = &gs->p1; def = &gs->p2; }
    else { att = &gs->p2; def = &gs->p1; }

    int sa = 0;
    int sb = 0;
    struct UnitStats s;

    for(int i=0; i<4; i++) {
        if (i==U_WORK) continue;
        get_stats(i, &s);
        sa += gs->battle.units[i] * s.attack;
    }

    for(int i=0; i<4; i++) {
        get_stats(i, &s);
        sb += def->units[i] * s.defense;
    }

    sa = sa / 10; 
    sb = sb / 10; 
    if(sa==0) sa=1;
    if(sb==0) sb=1;

    if (sa > sb) {
        for(int i=0; i<4; i++) def->units[i] = 0;
        att->points++;
    } else {
        for(int i=0; i<4; i++) {
            if (def->units[i] > 0) {
                long loss = (long)def->units[i] * sa;
                loss = loss / sb;
                def->units[i] -= (int)loss;
                if (def->units[i] < 0) def->units[i] = 0;
            }
        }
    }

    int def_att_power = 0;
    for(int i=0; i<4; i++) {
        get_stats(i, &s);
        if (i!=U_WORK) def_att_power += def->units[i] * s.attack;
    }
    def_att_power /= 10;

    int att_def_power = 0;
    for(int i=0; i<4; i++) {
        get_stats(i, &s);
        if (i!=U_WORK) att_def_power += gs->battle.units[i] * s.defense; 
    }
    att_def_power /= 10;
    if(def_att_power==0) def_att_power=1;
    if(att_def_power==0) att_def_power=1;

    if (def_att_power > att_def_power) {
        for(int i=0; i<4; i++) gs->battle.units[i] = 0;
    } else {
         for(int i=0; i<4; i++) {
            if (gs->battle.units[i] > 0) {
                long loss = (long)gs->battle.units[i] * def_att_power;
                loss = loss / att_def_power;
                gs->battle.units[i] -= (int)loss;
                if (gs->battle.units[i] < 0) gs->battle.units[i] = 0;
            }
        }
    }

    for(int i=0; i<4; i++) att->units[i] += gs->battle.units[i];

    gs->battle.active = 0;
}

void logic_handler(int sig) {
    sem_lock();
    gs->tick++;

    gs->p1.gold += 50 + (gs->p1.units[U_WORK] * 5);
    gs->p2.gold += 50 + (gs->p2.units[U_WORK] * 5);

    handle_training(&gs->p1, gs->tick);
    handle_training(&gs->p2, gs->tick);

    if (gs->battle.active && gs->tick >= gs->battle.tick_end) {
        resolve_battle();
    }

    if (gs->cmd_slot.active) {
        struct Player *p = (gs->cmd_slot.player_id == 1) ? &gs->p1 : &gs->p2;
        
        if (gs->cmd_slot.type == 'T') {
            if (p->training.active == 0) {
                struct UnitStats s;
                get_stats(gs->cmd_slot.u_type, &s);
                int total_cost = s.cost * gs->cmd_slot.count;
                if (p->gold >= total_cost) {
                    p->gold -= total_cost;
                    p->training.active = 1;
                    p->training.unit_type = gs->cmd_slot.u_type;
                    p->training.count = gs->cmd_slot.count;
                    p->training.next_tick = gs->tick + s.time;
                }
            }
        } else if (gs->cmd_slot.type == 'A') {
             if (gs->battle.active == 0 && gs->cmd_slot.u_type != U_WORK) {
                 int count = gs->cmd_slot.count;
                 if (p->units[gs->cmd_slot.u_type] >= count) {
                     p->units[gs->cmd_slot.u_type] -= count;
                     gs->battle.active = 1;
                     gs->battle.attacker_id = gs->cmd_slot.player_id;
                     gs->battle.tick_end = gs->tick + 5;
                     for(int k=0; k<4; k++) gs->battle.units[k] = 0;
                     gs->battle.units[gs->cmd_slot.u_type] = count;
                 }
             }
        }
        gs->cmd_slot.active = 0;
    }

    sem_unlock();
    alarm(1);
}

void process_comms() {
    struct MsgBuf mb;
    
    if (msgrcv(msgid, &mb, sizeof(struct MsgBuf)-sizeof(long), 1, IPC_NOWAIT) != -1) {
        sem_lock();
        if (gs->cmd_slot.active == 0) {
            gs->cmd_slot.active = 1;
            gs->cmd_slot.player_id = mb.player_id;
            gs->cmd_slot.type = mb.cmd_type;
            gs->cmd_slot.u_type = mb.u_type;
            gs->cmd_slot.count = mb.count;
        }
        sem_unlock();
    }

    if (msgrcv(msgid, &mb, sizeof(struct MsgBuf)-sizeof(long), 2, IPC_NOWAIT) != -1) {
        sem_lock();
        if (gs->cmd_slot.active == 0) {
            gs->cmd_slot.active = 1;
            gs->cmd_slot.player_id = mb.player_id;
            gs->cmd_slot.type = mb.cmd_type;
            gs->cmd_slot.u_type = mb.u_type;
            gs->cmd_slot.count = mb.count;
        }
        sem_unlock();
    }

    sem_lock();
    struct MsgBuf update;
    update.mtype = 10;
    my_memcpy(update.raw_text, &gs->p1, sizeof(struct Player));
    msgsnd(msgid, &update, sizeof(struct MsgBuf)-sizeof(long), 0);
    
    update.mtype = 20;
    my_memcpy(update.raw_text, &gs->p2, sizeof(struct Player));
    msgsnd(msgid, &update, sizeof(struct MsgBuf)-sizeof(long), 0);
    sem_unlock();
}

void write_str(const char *s) {
    int len = 0; while(s[len]) len++;
    write(1, s, len);
}

int main() {
    shmid = shmget(KEY_SHM, sizeof(struct GameState), IPC_CREAT | 0666);
    gs = (struct GameState*)shmat(shmid, NULL, 0);
    
    semid = semget(KEY_SEM, 1, IPC_CREAT | 0666);
    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    msgid = msgget(KEY_MSG, IPC_CREAT | 0666);

    sem_lock();
    gs->tick = 0;
    gs->p1.id = 1; gs->p1.gold = 300; gs->p1.points = 0; gs->p1.training.active = 0;
    gs->p2.id = 2; gs->p2.gold = 300; gs->p2.points = 0; gs->p2.training.active = 0;
    for(int i=0; i<4; i++) { gs->p1.units[i]=0; gs->p2.units[i]=0; }
    gs->battle.active = 0;
    gs->cmd_slot.active = 0;
    sem_unlock();

    if (fork() == 0) {
        signal(SIGALRM, logic_handler);
        alarm(1);
        while(1) {
            pause();
        }
    } else {
        write_str("Serwer wystartowal.\n");
        while(1) {
            process_comms();
            usleep(100000); 
        }
    }
    return 0;
}