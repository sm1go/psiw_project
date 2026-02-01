#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "common.h"

int msgid;
pid_t pid_dziecka = 0;

// Funkcja pisząca tekst na ekran
void pisz(const char *s) {
    write(1, s, strlen(s));
}

// Funkcja pomocnicza: konwersja int -> tekst i wypisanie
void pisz_int(int n) {
    if (n == 0) {
        write(1, "0", 1);
        return;
    }
    char buf[16];
    int i = 0;
    int temp = n;
    while(temp > 0) {
        temp /= 10;
        i++;
    }
    buf[i] = 0;
    temp = n;
    while(temp > 0) {
        buf[--i] = (temp % 10) + '0';
        temp /= 10;
    }
    write(1, buf, strlen(buf));
}

// Funkcja czyszcząca ekran kodem ANSI (system("clear") też jest w gracz.c, więc dozwolone)
void clear_screen() {
    write(1, "\033[H\033[J", 6);
}

// Własna implementacja czytania liczby (zamiast scanf)
int czytaj_int() {
    char buf[16];
    int i = 0;
    char c;
    while(read(0, &c, 1) > 0) {
        if (c == '\n' || c == ' ') break;
        if (c >= '0' && c <= '9' && i < 15) {
            buf[i++] = c;
        }
    }
    buf[i] = 0;
    if (i == 0) return 0;
    
    // Prosta konwersja string -> int
    int wynik = 0;
    int mnoznik = 1;
    for(int k=i-1; k>=0; k--) {
        wynik += (buf[k] - '0') * mnoznik;
        mnoznik *= 10;
    }
    return wynik;
}

void sprzatanie(int sig) {
    if (pid_dziecka > 0) kill(pid_dziecka, SIGKILL);
    exit(0);
}

int main() {
    signal(SIGINT, sprzatanie);
    msgid = msgget(KLUCZ_MSG, 0600);
    if (msgid == -1) {
        pisz("Blad polaczenia z serwerem.\n");
        return 1;
    }

    pid_t my_pid = getpid();

    struct msgbuf logowanie;
    logowanie.mtype = MSG_LOGIN;
    logowanie.id_nadawcy = my_pid;
    msgsnd(msgid, &logowanie, sizeof(logowanie)-sizeof(long), 0);
    
    pisz("Oczekiwanie na gre...\n");

    pid_dziecka = fork();
    if (pid_dziecka == 0) {
        // PROCES ODBIERAJĄCY (Interfejs)
        struct msgupdate info;

        while(1) {
            if (msgrcv(msgid, &info, sizeof(info)-sizeof(long), 10 + my_pid, 0) != -1) {
                // Rysowanie interfejsu bez sprintf
                clear_screen();
                
                pisz("=== TWOJA BAZA === [Punkty: "); pisz_int(info.ja.punkty); pisz("]\n");
                pisz("Surowce: "); pisz_int(info.ja.surowce); pisz("\n");
                pisz("Jednostki:\n");
                pisz("  Lekka: "); pisz_int(info.ja.jednostki[0]); pisz("\n");
                pisz("  Ciezka: "); pisz_int(info.ja.jednostki[1]); pisz("\n");
                pisz("  Jazda: "); pisz_int(info.ja.jednostki[2]); pisz("\n");
                pisz("  Robotnicy: "); pisz_int(info.ja.jednostki[3]); pisz("\n");
                
                pisz("W produkcji (czas):\n");
                pisz("  L: "); pisz_int(info.ja.w_produkcji[0]); 
                pisz(" ("); pisz_int(info.ja.czas_produkcji[0]); pisz("s)\n");
                
                pisz("\n=== PRZECIWNIK === [Punkty: "); pisz_int(info.wrog.punkty); pisz("]\n");
                pisz("  Lekka: "); pisz_int(info.wrog.jednostki[0]); pisz("\n");
                pisz("  Ciezka: "); pisz_int(info.wrog.jednostki[1]); pisz("\n");
                pisz("  Jazda: "); pisz_int(info.wrog.jednostki[2]); pisz("\n");

                pisz("\nOSTATNI KOMUNIKAT: "); pisz(info.wiadomosc); pisz("\n");
                pisz("\n[1] Buduj [2] Atak (Ctrl+C - Wyjscie)\nWybor: ");
            }
        }
    } else {
        // PROCES RODZICA (Klawiatura)
        struct msgbuf rozkaz;
        rozkaz.id_nadawcy = my_pid;
        rozkaz.mtype = MSG_AKCJA;

        while(1) {
            int opcja = czytaj_int();
            
            kill(pid_dziecka, SIGSTOP); // Pauza odświeżania

            if (opcja == 1) {
                pisz("\n-- BUDOWA --\nTyp (0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik): ");
                int typ = czytaj_int();
                pisz("Ilosc: ");
                int ilosc = czytaj_int();
                
                rozkaz.typ_akcji = 1;
                rozkaz.parametry[0] = typ;
                rozkaz.parametry[1] = ilosc;
                msgsnd(msgid, &rozkaz, sizeof(rozkaz)-sizeof(long), 0);
            } 
            else if (opcja == 2) {
                pisz("\n-- ATAK --\n");
                pisz("Lekka: "); int l = czytaj_int();
                pisz("Ciezka: "); int c = czytaj_int();
                pisz("Jazda: "); int j = czytaj_int();

                rozkaz.typ_akcji = 2;
                rozkaz.parametry[0] = l;
                rozkaz.parametry[1] = c;
                rozkaz.parametry[2] = j;
                msgsnd(msgid, &rozkaz, sizeof(rozkaz)-sizeof(long), 0);
            }

            kill(pid_dziecka, SIGCONT); // Wznowienie odświeżania
        }
    }
}