#include "common.h"

int msgid;
int my_id;

void str_write(const char *s) {
    int len = 0;
    while(s[len]) len++;
    write(1, s, len);
}

void int_to_str(int n, char *buf) {
    int i = 0, temp = n;
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    int len = 0;
    while(temp>0) { temp/=10; len++; }
    for(i=0; i<len; i++) {
        buf[len-1-i] = (n%10) + '0';
        n/=10;
    }
    buf[len] = '\0';
}

void print_num(int n) {
    char buf[16];
    int_to_str(n, buf);
    str_write(buf);
}

int str_to_int(char *s) {
    int res = 0;
    while(*s >= '0' && *s <= '9') {
        res = res*10 + (*s - '0');
        s++;
    }
    return res;
}

void display_state(struct Player *p) {
    str_write("\n------------------------\n");
    str_write("Gracz "); print_num(p->id); str_write("\n");
    str_write("Zloto: "); print_num(p->gold); str_write("\n");
    str_write("Punkty: "); print_num(p->points); str_write("\n");
    str_write("Jednostki:\n");
    str_write(" Lekka: "); print_num(p->units[U_LIGHT]); str_write("\n");
    str_write(" Ciezka: "); print_num(p->units[U_HEAVY]); str_write("\n");
    str_write(" Jazda: "); print_num(p->units[U_CAV]); str_write("\n");
    str_write(" Robotnicy: "); print_num(p->units[U_WORK]); str_write("\n");
    if (p->training.active) {
        str_write("Trening typ: "); print_num(p->training.unit_type);
        str_write(" pozostalo: "); print_num(p->training.count); str_write("\n");
    } else {
        str_write("Brak aktywnego treningu\n");
    }
    str_write("------------------------\n");
}

int main(int argc, char *argv[]) {
    msgid = msgget(KEY_MSG, 0666);
    
    if (argc > 1) my_id = 2; else my_id = 1;

    str_write("Start Klienta. ID: "); print_num(my_id); str_write("\n");
    str_write("Komendy: t [typ] [ilosc] (trening), a [typ] [ilosc] (atak)\n");
    str_write("Typy: 0-Lekka, 1-Ciezka, 2-Jazda, 3-Rob\n");

    if (fork() == 0) {
        char buf[100];
        while(1) {
            int n = read(0, buf, 99);
            if (n > 0) {
                buf[n] = 0;
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
                }
            }
        }
    } else {
        struct MsgBuf mb;
        struct Player p;
        long listen_type = (my_id == 1) ? 10 : 20;
        
        while(1) {
            if (msgrcv(msgid, &mb, sizeof(struct MsgBuf)-sizeof(long), listen_type, 0) != -1) {
                char *src = mb.raw_text;
                char *dest = (char *)&p;
                for(int i=0; i<sizeof(struct Player); i++) dest[i] = src[i];
                display_state(&p);
            }
        }
    }
    return 0;
}