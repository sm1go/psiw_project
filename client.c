#include "common.h"

int msgid;
int my_id;
pid_t pid_dziecka; // Musimy znac PID dziecka zeby wysylac mu sygnaly

/* Funkcje pomocnicze do pisania */
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

/* Funkcja wyswietlajaca stan gry (uruchamiana w procesie dziecka) */
void display_state(struct Player *p) {
    char buf[512];
    int pos = 0;

    /* Czytelny format, ktory bedzie sie przewijal */
    append_str(buf, &pos, "\n--- STAN GRY (Gracz "); append_num(buf, &pos, p->id);
    append_str(buf, &pos, ") ---\n");
    append_str(buf, &pos, " Zloto:   "); append_num(buf, &pos, p->gold);
    append_str(buf, &pos, "\n Punkty:  "); append_num(buf, &pos, p->points);
    append_str(buf, &pos, "\n Armia:   L:"); append_num(buf, &pos, p->units[U_LIGHT]);
    append_str(buf, &pos, " C:"); append_num(buf, &pos, p->units[U_HEAVY]);
    append_str(buf, &pos, " J:"); append_num(buf, &pos, p->units[U_CAV]);
    append_str(buf, &pos, " R:"); append_num(buf, &pos, p->units[U_WORK]);
    
    if (p->training.active) {
        append_str(buf, &pos, "\n TRENING: Typ "); append_num(buf, &pos, p->training.unit_type);
        append_str(buf, &pos, " (pozostalo "); append_num(buf, &pos, p->training.count);
        append_str(buf, &pos, ")");
    } else {
        append_str(buf, &pos, "\n TRENING: Wolny");
    }
    append_str(buf, &pos, "\n--------------------------\n");
    append_str(buf, &pos, "[Wcisnij ENTER aby wpisac komende]\n"); // Instrukcja caly czas widoczna
    
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

/* Handler do sprzatania procesow przy wyjsciu (Ctrl+C) */
void cleanup(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup); // Zeby zabic dziecko przy zamykaniu
    msgid = msgget(KEY_MSG, 0666);
    
    if (argc > 1) my_id = 2; else my_id = 1;

    raw_write("KLIENT START.\n");

    pid_dziecka = fork();

    /* --- PROCES DZIECKA: TYLKO WYSWIETLA --- */
    if (pid_dziecka == 0) {
        struct MsgBuf mb;
        struct Player p;
        long listen_type = (my_id == 1) ? 10 : 20;
        
        while(1) {
            // Czekamy na wiadomosc od serwera
            if (msgrcv(msgid, &mb, sizeof(struct MsgBuf)-sizeof(long), listen_type, 0) != -1) {
                char *src = mb.raw_text;
                char *dest = (char *)&p;
                for(int i=0; i<sizeof(struct Player); i++) dest[i] = src[i];
                
                display_state(&p);
            }
        }
    } 
    /* --- PROCES RODZICA: TYLKO WPISYWANIE --- */
    else {
        char trigger_buf[10];
        char cmd_buf[100];
        
        while(1) {
            // 1. Czekamy na jakikolwiek klawisz (np. ENTER) zeby zatrzymac ekran
            // To czyta ten "pusty" enter
            read(0, trigger_buf, 10); 

            // 2. ZATRZYMUJEMY WYSWIETLANIE!
            kill(pid_dziecka, SIGSTOP);

            // 3. Teraz mamy cisze na ekranie. Mozemy pytac o komende.
            raw_write("\n========================================\n");
            raw_write(" PAUZA - WPISYWANIE KOMENDY\n");
            raw_write(" Format: t [typ] [ilosc]  lub  a [typ] [ilosc]\n");
            raw_write(" (typ: 0-Lekka, 1-Ciezka, 2-Jazda, 3-Rob)\n");
            raw_write(" > "); // Znak zachety
            
            // 4. Czytamy wlasciwa komende
            int n = read(0, cmd_buf, 99);
            if (n > 0) {
                cmd_buf[n] = 0;
                
                struct MsgBuf msg;
                msg.mtype = my_id;
                msg.player_id = my_id;
                
                char type = cmd_buf[0];
                int u_type = -1;
                int count = 0;
                
                // Parsowanie komendy (np. "t 0 5")
                // Zakladamy spacje po literze
                int i = 1;
                while(cmd_buf[i] == ' ') i++; // pomin spacje
                
                if (cmd_buf[i] >= '0' && cmd_buf[i] <= '3') {
                    u_type = cmd_buf[i] - '0';
                    i++;
                }

                while(cmd_buf[i] == ' ') i++; // pomin spacje
                if (cmd_buf[i] >= '0' && cmd_buf[i] <= '9') {
                    count = str_to_int(&cmd_buf[i]);
                }

                if ((type == 't' || type == 'a') && u_type >= 0 && count > 0) {
                    msg.cmd_type = (type == 't') ? 'T' : 'A';
                    msg.u_type = u_type;
                    msg.count = count;
                    msgsnd(msgid, &msg, sizeof(struct MsgBuf)-sizeof(long), 0);
                    raw_write(" [KOMENDA WYSLANA]\n");
                } else {
                    raw_write(" [!] BLAD KOMENDY (anulowano)\n");
                }
            }

            raw_write(" Wracam do gry...\n");
            raw_write("========================================\n");
            
            // 5. WZNAWIAMY WYSWIETLANIE
            kill(pid_dziecka, SIGCONT);
        }
    }
    return 0;
}