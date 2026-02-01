#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "common.h"

int msg_id;
int my_id = -1;
pid_t my_pid;

// Funkcja pomocnicza do pisania na ekran
void my_write(const char* str) {
    write(1, str, strlen(str));
}

// Inicjalizacja ekranu - czysci calosc i rysuje statyczny uklad raz
void setup_screen() {
    char buf[512];
    sprintf(buf, "\033[H\033[J"); // Wyczysc ekran
    write(1, buf, strlen(buf));
    
    // Naglowek (Linia 1)
    sprintf(buf, "=== KLIENT GRY === (PID: %d)\n", my_pid);
    write(1, buf, strlen(buf));
    
    // Statystyki (Linia 2) - to bedziemy aktualizowac
    sprintf(buf, "OCZEKIWANIE NA DANE...\n");
    write(1, buf, strlen(buf));
    
    // Kreska (Linia 3)
    sprintf(buf, "------------------------------------------------\n");
    write(1, buf, strlen(buf));
    
    // Instrukcja (Linie 4-6)
    sprintf(buf, "Komendy:\n b [typ 0-3] [ilosc] - buduj (0:Lekka, 1:Ciezka, 2:Jazda, 3:Rob)\n a [l] [c] [j] - atakuj\n");
    write(1, buf, strlen(buf));

    // Miejsce na komunikaty (Linia 8)
    write(1, "\033[8;1HKOMUNIKATY: Brak\n", 22);

    // Ustawienie kursora w linii zachety (Linia 10)
    write(1, "\033[10;1H> ", 8);
}

// Funkcja aktualizujaca TYLKO linie ze statystykami
void update_stats(int gold, int u1, int u2, int u3) {
    char buf[256];
    
    // 1. Zapisz obecna pozycje kursora (tam gdzie uzytkownik pisze)
    write(1, "\0337", 2); 
    
    // 2. Skocz do Linii 2 (tam sa statystyki)
    write(1, "\033[2;1H", 6);
    
    // 3. Nadpisz linie i wyczysc reszte linii (\033[K) zeby nie zostaly smieci
    sprintf(buf, "ZLOTO: %d | LEKKA: %d | CIEZKA: %d | JAZDA: %d   \033[K", gold, u1, u2, u3);
    write(1, buf, strlen(buf));
    
    // 4. Przywroc kursor na dol (tam gdzie uzytkownik pisal)
    write(1, "\0338", 2);
}

// Funkcja aktualizujaca TYLKO linie komunikatow
void update_message(char* text) {
    char buf[300];
    write(1, "\0337", 2); // Zapisz kursor
    write(1, "\033[8;1H", 6); // Skok do linii 8
    sprintf(buf, "KOMUNIKAT: %s\033[K", text);
    write(1, buf, strlen(buf));
    write(1, "\0338", 2); // Przywroc kursor
}

void receiver_loop() {
    Message msg;
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message) - sizeof(long), my_pid, 0) > 0) {
            if (msg.cmd == MSG_STATE) {
                update_stats(msg.args[0], msg.args[1], msg.args[2], msg.args[3]);
            } else if (msg.cmd == MSG_RESULT || msg.cmd == MSG_GAME_OVER) {
                update_message(msg.text);
                if (msg.cmd == MSG_GAME_OVER) {
                    kill(getppid(), SIGKILL); // Zabij proces rodzica (wejscia)
                    exit(0);
                }
            } else if (msg.cmd == CMD_LOGIN) {
                my_id = msg.args[0];
            }
        }
    }
}

int main() {
    my_pid = getpid();
    key_t key = ftok(".", KEY_ID);
    msg_id = msgget(key, 0666);
    
    if (msg_id == -1) {
        my_write("Brak kolejki komunikatow. Uruchom serwer.\n");
        return 1;
    }

    // Logowanie
    Message login;
    login.mtype = 1; 
    login.src_id = my_pid;
    login.cmd = CMD_LOGIN;
    msgsnd(msg_id, &login, sizeof(Message) - sizeof(long), 0);

    // Rysujemy interfejs raz na poczatku
    setup_screen();

    if (fork() == 0) {
        receiver_loop();
        exit(0);
    }

    char input[100];
    while(1) {
        // Czekaj na wpisanie komendy
        int n = read(0, input, 99);
        if (n > 0) {
            input[n] = 0; // Null-terminate
            
            // Parsowanie
            char cmd;
            int a1, a2, a3;
            // Skanujemy bufor
            if (sscanf(input, "%c %d %d %d", &cmd, &a1, &a2, &a3) >= 1) {
                Message ord;
                ord.mtype = 1;
                ord.src_id = my_pid;
                
                if (cmd == 'b') {
                    ord.cmd = CMD_BUILD;
                    ord.args[0] = a1; 
                    ord.args[1] = a2; 
                    msgsnd(msg_id, &ord, sizeof(Message) - sizeof(long), 0);
                } else if (cmd == 'a') {
                    ord.cmd = CMD_ATTACK;
                    ord.args[0] = a1;
                    ord.args[1] = a2;
                    ord.args[2] = a3;
                    msgsnd(msg_id, &ord, sizeof(Message) - sizeof(long), 0);
                }
            }
            
            // Po wcisnieciu ENTER:
            // 1. Wyczysc linie komend (Linia 10)
            // 2. Wypisz zachete ponownie
            write(1, "\033[10;1H\033[K> ", 11); 
        }
    }
}