#include "common.h"

int msgid;
int my_id;

void raw_write(const char *s) {
    int len = 0;
    while(s[len]) len++;
    write(1, s, len);
}

void append_num(char *buf, int *pos, int n) {
    char temp[16];
    int i = 0, t = n;
    if (n == 0) {
        temp[0] = '0'; i = 1;
    } else {
        while(t > 0) { t /= 10; i++; }
        t = n;
        for(int j = 0; j < i; j++) {
            temp[i - 1 - j] = (t % 10) + '0';
            t /= 10;
        }
    }
    for(int j = 0; j < i; j++) {
        buf[*pos] = temp[j];
        (*pos)++;
    }
}

void append_str(char *buf, int *pos, const char *s) {
    int i = 0;
    while(s[i]) {
        buf[*pos] = s[i];
        (*pos)++;
        i++;
    }
}

void display_line(struct Player *p) {
    char buf[256];
    int pos = 0;

    append_str(buf, &pos, "[Gracz "); append_num(buf, &pos, p->id);
    append_str(buf, &pos, "] Zloto: "); append_num(buf, &pos, p->gold);
    append_str(buf, &pos, " | PKT: "); append_num(buf, &pos, p->points);
    append_str(buf, &pos, " | Armia L/C/J/R: ");
    append_num(buf, &pos, p->units[U_LIGHT]); append_str(buf, &pos, "/");
    append_num(buf, &pos, p->units[U_HEAVY]); append_str(buf, &pos, "/");
    append_num(buf, &pos, p->units[U_CAV]); append_str(buf, &pos, "/");
    append_num(buf, &pos, p->units[U_WORK]);

    if (p->training.active) {
        append_str(buf, &pos, " | TRENING T-");
        append_num(buf, &pos, p->training.unit_type);
        append_str(buf, &pos, " (");
        append_num(buf, &pos, p->training.count);
        append_str(buf, &pos, ")");
    }
    append_str(buf, &pos, "\n");
    write(1, buf, pos);
}

void display_full_table(struct Player *p) {
    char buf[512];
    int pos = 0;
    append_str(buf, &pos, "\n--- PELNY RAPORT ---\n");
    append_str(buf, &pos, "Gracz: "); append_num(buf, &pos, p->id);
    append_str(buf, &pos, "\nZloto: "); append_num(buf, &pos, p->gold);
    append_str(buf, &pos, "\nPunkty: "); append_num(buf, &pos, p->points);
    append_str(buf, &pos, "\nJednostki:\n 0-Lekka: "); append_num(buf, &pos, p->units[U_LIGHT]);
    append_str(buf, &pos, "\n 1-Ciezka: "); append_num(buf, &pos, p->units[U_HEAVY]);
    append_str(buf, &pos, "\n 2-Jazda:  "); append_num(buf, &pos, p->units[U_CAV]);
    append_str(buf, &pos, "\n 3-Robol:  "); append_num(buf, &pos, p->units[U_WORK]);
    append_str(buf, &pos, "\n--------------------\n");
    write(1, buf, pos);
}

int str_to_int(char *s) {
    int res = 0;
    while(*s >= '0' && *s <= '9') {
        res = res*10 + (*s - '0');
        s++;
    }
    return res;
}

int main(int argc, char *argv[]) {
    msgid = msgget(KEY_MSG, 0666);
    
    if (argc > 1) my_id = 2; else my_id = 1;

    raw_write("Klient uruchomiony.\n");
    raw_write("Komendy:\n t [typ] [ilosc] -> trening\n a [typ] [ilosc] -> atak\n s -> pokaz tabele\n");

    if (fork() == 0) {
        char buf[100];
        while(1) {
            int n = read(0, buf, 99);
            if (n > 0) {
                buf[n] = 0;
                if (buf[0] == 's') {
                    struct MsgBuf msg;
                    msg.mtype = my_id;
                    msg.player_id = my_id;
                    msg.cmd_type = 'S'; 
                    msgsnd(msgid, &msg, sizeof(struct MsgBuf)-sizeof(long), 0);
                    continue;
                }

                struct MsgBuf msg;
                msg.mtype = my_id;
                msg.player_id = my_id;
                
                char type = buf[0];
                int u_type = -1;
                int count = 0;
                
                int i = 2;
                if (buf[1] == ' ') {
                    u_type = buf[i] - '0';
                    i += 2;
                    count = str_to_int(&buf[i]);
                }

                if ((type == 't' || type == 'a') && u_type >= 0 && count > 0) {
                    msg.cmd_type = (type == 't') ? 'T' : 'A';
                    msg.u_type = u_type;
                    msg.count = count;
                    msgsnd(msgid, &msg, sizeof(struct MsgBuf)-sizeof(long), 0);
                    raw_write(" [Wyslano]\n");
                }
            }
        }
    } else {
        struct MsgBuf mb;
        struct Player p;
        long listen_type = (my_id == 1) ? 10 : 20;
        
        int show_full = 0;

        while(1) {
            if (msgrcv(msgid, &mb, sizeof(struct MsgBuf)-sizeof(long), listen_type, 0) != -1) {
                char *src = mb.raw_text;
                char *dest = (char *)&p;
                for(int i=0; i<sizeof(struct Player); i++) dest[i] = src[i];
                
                struct MsgBuf cmd_check;
                if (msgrcv(msgid, &cmd_check, sizeof(struct MsgBuf)-sizeof(long), my_id, IPC_NOWAIT) != -1) {
                    if (cmd_check.cmd_type == 'S') show_full = 1;
                    else msgsnd(msgid, &cmd_check, sizeof(struct MsgBuf)-sizeof(long), 0); 
                }

                if (show_full) {
                    display_full_table(&p);
                    show_full = 0;
                } else {
                    display_line(&p);
                }
            }
        }
    }
    return 0;
}