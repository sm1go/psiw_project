#include "common.h"

// Pomocnicze funkcje I/O na bazie pliki.pdf [cite: 384]
void write_str(const char* s) {
    int len = 0; while(s[len]) len++;
    write(1, s, len);
}

void write_int(int n) {
    char buf[16]; int i = 0;
    if(n == 0) { write(1, "0", 1); return; }
    while(n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while(i--) write(1, &buf[i], 1);
}

int read_int() {
    char buf[16];
    int n = read(0, buf, 15);
    buf[n] = 0;
    return atoi(buf);
}

pid_t pid_dziecka;
void cleanup(int s) { if(pid_dziecka > 0) kill(pid_dziecka, SIGKILL); exit(0); }

int main() {
    signal(SIGINT, cleanup);
    int msg = msgget(KLUCZ, 0666);
    MsgBuf m;
    
    // Logowanie
    m.mtype = MSG_LOGOWANIE;
    m.id_gracza = getpid();
    m.wybor = MSG_LOGOWANIE;
    msgsnd(msg, &m, sizeof(m)-sizeof(long), 0);
    msgrcv(msg, &m, sizeof(m)-sizeof(long), getpid(), 0);
    
    int my_id = m.id_gracza;
    if(my_id == -1) { write_str("Serwer pelny!\n"); return 1; }

    pid_dziecka = fork();
    if(pid_dziecka == 0) { // PROCES WYSWIETLAJACY
        while(1) {
            m.mtype = MSG_STAN;
            m.id_gracza = my_id;
            m.wybor = MSG_STAN;
            msgsnd(msg, &m, sizeof(m)-sizeof(long), 0);
            msgrcv(msg, &m, sizeof(m)-sizeof(long), my_id, 0);

            write_str("\n--- STAN GRY ---\nZasoby: "); write_int(m.zasoby);
            write_str(" | Punkty: "); write_int(m.punkty);
            write_str("\nWojsko: L:"); write_int(m.jednostki[0]);
            write_str(" C:"); write_int(m.jednostki[1]);
            write_str(" J:"); write_int(m.jednostki[2]);
            write_str(" R:"); write_int(m.jednostki[3]);
            write_str("\n1. Buduj, 2. Atak, 3. Wyjdz\nWybor: ");
            sleep(2);
        }
    } else { // PROCES INPUTU
        while(1) {
            int opcja = read_int();
            kill(pid_dziecka, SIGSTOP);
            if(opcja == 1) {
                write_str("Typ (0-L, 1-C, 2-J, 3-R) i ilosc: ");
                m.mtype = MSG_BUDUJ; m.id_gracza = my_id; m.wybor = MSG_BUDUJ;
                m.jednostki[0] = read_int(); m.jednostki[1] = read_int();
                msgsnd(msg, &m, sizeof(m)-sizeof(long), 0);
            } else if(opcja == 2) {
                write_str("Atak - podaj ilosc L C J: ");
                m.mtype = MSG_ATAK; m.id_gracza = my_id; m.wybor = MSG_ATAK;
                m.jednostki[0] = read_int(); m.jednostki[1] = read_int(); m.jednostki[2] = read_int();
                msgsnd(msg, &m, sizeof(m)-sizeof(long), 0);
            } else if(opcja == 3) cleanup(0);
            kill(pid_dziecka, SIGCONT);
        }
    }
    return 0;
}