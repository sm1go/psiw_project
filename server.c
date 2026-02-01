#include "common.h"

int msg_id, shm_id, sem_id;
SharedGameState *game_state;

void shutdown_server(int sig) {
    msgctl(msg_id, IPC_RMID, NULL);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    exit(0);
}

int main() {
    signal(SIGINT, shutdown_server);
    signal(SIGCHLD, SIG_IGN);

    msg_id = msgget(PROJECT_KEY, IPC_CREAT | 0600);
    shm_id = shmget(PROJECT_KEY, sizeof(SharedGameState), IPC_CREAT | 0600);
    sem_id = semget(PROJECT_KEY, 1, IPC_CREAT | 0600);

    union semun arg;
    arg.val = 1;
    semctl(sem_id, 0, SETVAL, arg);

    game_state = (SharedGameState*)shmat(shm_id, NULL, 0);
    memset(game_state, 0, sizeof(SharedGameState));
    game_state->player_resources[0] = 300;
    game_state->player_resources[1] = 300;

    // Proces generowania surowców
    if (fork() == 0) {
        while(1) {
            sleep(1);
            lock_shm(sem_id);
            if (game_state->is_game_started) {
                for(int i=0; i<2; i++) {
                    game_state->player_resources[i] += 50 + (game_state->player_army[i][3] * 5);
                }
            }
            unlock_shm(sem_id);
        }
    }

    int players_ready = 0;
    GameMessage msg;

    while(1) {
        msgrcv(msg_id, &msg, sizeof(GameMessage)-sizeof(long), TYPE_SERVER, 0);

        if (msg.action == ACTION_LOGIN) {
            lock_shm(sem_id);
            msg.mtype = msg.player_id; 
            if (players_ready < 2) {
                msg.player_id = ++players_ready;
                if (players_ready == 2) game_state->is_game_started = 1;
            } else msg.player_id = -1;
            msgsnd(msg_id, &msg, sizeof(GameMessage)-sizeof(long), 0);
            unlock_shm(sem_id);
        } 
        else if (msg.action == ACTION_STATE) {
            int p = msg.player_id - 1;
            lock_shm(sem_id);
            msg.mtype = msg.player_id;
            msg.resource_info = game_state->player_resources[p];
            msg.score_info = game_state->player_scores[p];
            msg.game_status = game_state->is_game_started;
            for(int i=0; i<4; i++) msg.army_info[i] = game_state->player_army[p][i];
            
            // Sprawdzenie czy przeciwnik wygrał
            int enemy = (p == 0) ? 1 : 0;
            if (game_state->player_scores[enemy] >= WIN_THRESHOLD) msg.score_info = -1; 

            msgsnd(msg_id, &msg, sizeof(GameMessage)-sizeof(long), 0);
            unlock_shm(sem_id);
        }
        else if (msg.action == ACTION_BUILD) {
            int p = msg.player_id - 1;
            int total_cost = UNIT_STATS[msg.unit_type].cost * msg.quantity;
            lock_shm(sem_id);
            if (game_state->player_resources[p] >= total_cost) {
                game_state->player_resources[p] -= total_cost;
                unlock_shm(sem_id);
                if (fork() == 0) {
                    for(int i=0; i<msg.quantity; i++) {
                        sleep(UNIT_STATS[msg.unit_type].build_time);
                        lock_shm(sem_id);
                        game_state->player_army[p][msg.unit_type]++;
                        unlock_shm(sem_id);
                    }
                    exit(0);
                }
            } else unlock_shm(sem_id);
        }
        else if (msg.action == ACTION_ATTACK) {
            int p = msg.player_id - 1;
            int enemy = (p == 0) ? 1 : 0;
            lock_shm(sem_id);
            int valid = 1;
            for(int i=0; i<3; i++) if(game_state->player_army[p][i] < msg.army_info[i]) valid = 0;

            if(valid) {
                for(int i=0; i<3; i++) game_state->player_army[p][i] -= msg.army_info[i];
                unlock_shm(sem_id);
                if(fork() == 0) {
                    int attackers[4];
                    for(int i=0; i<4; i++) attackers[i] = msg.army_info[i];
                    sleep(5); 
                    lock_shm(sem_id);
                    float SA = 0, SB = 0, ESA = 0, ESB = 0;
                    for(int i=0; i<3; i++) {
                        SA += attackers[i] * UNIT_STATS[i].attack_val;
                        SB += game_state->player_army[enemy][i] * UNIT_STATS[i].defense_val;
                        ESA += game_state->player_army[enemy][i] * UNIT_STATS[i].attack_val;
                        ESB += attackers[i] * UNIT_STATS[i].defense_val;
                    }
                    if (SA > SB) {
                        game_state->player_scores[p]++;
                        for(int i=0; i<3; i++) game_state->player_army[enemy][i] = 0;
                    } else {
                        float r = (SB > 0) ? SA/SB : 0;
                        for(int i=0; i<3; i++) game_state->player_army[enemy][i] -= (int)(game_state->player_army[enemy][i] * r);
                    }
                    if (ESA > ESB) { for(int i=0; i<3; i++) attackers[i] = 0; }
                    else {
                        float r = (ESB > 0) ? ESA/ESB : 0;
                        for(int i=0; i<3; i++) attackers[i] -= (int)(attackers[i] * r);
                    }
                    for(int i=0; i<3; i++) game_state->player_army[p][i] += attackers[i];
                    unlock_shm(sem_id);
                    exit(0);
                }
            } else unlock_shm(sem_id);
        }
    }
}