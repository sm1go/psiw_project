#include "common.h"

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

void write_str(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

// Funkcja: int na string (wlasna implementacja)
void append_int(char *dest, int n) {
    char temp[16];
    int i = 0;
    if (n == 0) {
        temp[i++] = '0';
    } else {
        while (n > 0) {
            temp[i++] = (n % 10) + '0';
            n /= 10;
        }
    }
    temp[i] = '\0';
    for(int j=0; j<i/2; j++) {
        char t = temp[j];
        temp[j] = temp[i-1-j];
        temp[i-1-j] = t;
    }
    strcat(dest, temp);
}

// Funkcja: string na int (wlasna implementacja)
int str_to_int(char* str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

// PROCES POTOMNY: Odpowiada tylko za wyswietlanie stanu gry (GORA EKRANU)
void display_loop() {
    Message msg;
    char out_buf[4096];
    char line_buf[512];

    while(1) {
        // Czekaj na wiadomosc od serwera (blokujaco)
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, 0) == -1) {
            exit(0);
        }

        if (msg.cmd == CMD_UPDATE) {
            // \0337 - ZAPISZ pozycje kursora (tam gdzie uzytkownik wlasnie pisze)
            // \033[?25l - Ukryj kursor (zeby nie migal na gorze)
            out_buf[0] = '\0';
            strcat(out_buf, "\0337\033[?25l");
            
            // Przesun kursor na gore (1,1) i rysuj statystyki
            strcat(out_buf, "\033[1;1H");
            
            strcat(out_buf, "=== GRACZ ");
            append_int(out_buf, my_player_id + 1);
            strcat(out_buf, " ===\033[K\n"); // \033[K czysci reszte linii

            char *line = strtok(msg.text, "\n");
            while(line != NULL) {
                strcat(out_buf, line);
                strcat(out_buf, "\033[K\n");
                line = strtok(NULL, "\n");
            }
            
            strcat(out_buf, "------------------------------------------------\033[K\n");
            strcat(out_buf, "KOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\033[K\n");
            strcat(out_buf, "(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\033[K\n");
            strcat(out_buf, "------------------------------------------------\033[K");

            // \0338 - PRZYWROC pozycje kursora na dol
            // \033[?25h - Pokaz kursor
            strcat(out_buf, "\0338\033[?25h");
            
            write_str(out_buf);

        } else if (msg.cmd == CMD_RESULT) {
            // Koniec gry - zabijamy rodzica (input) i konczymy
            out_buf[0] = '\0';
            strcat(out_buf, "\033[2J\033[H\n=== KONIEC GRY ===\n");
            strcat(out_buf, msg.text);
            strcat(out_buf, "\n\n");
            write_str(out_buf);
            kill(getppid(), SIGKILL);
            exit(0);
        }
    }
}

// PROCES RODZICA: Odpowiada tylko za wpisywanie komend (DOL EKRANU)
void input_loop() {
    char buffer[256];
    int pos = 0;
    char c;

    // Przesun kursor na dol (linia 15), zeby nie pisac po statystykach na start
    write_str("\033[15;1H> ");

    while(1) {
        // Czytaj jeden znak
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') { // ENTER
                write_str("\n> "); // Nowa linia wizualnie
                
                buffer[pos] = '\0';
                
                // Parsowanie komendy (recznie, bez sscanf)
                char temp_buf[256];
                strcpy(temp_buf, buffer);
                
                char *token = strtok(temp_buf, " ");
                if (token != NULL) {
                    Message msg;
                    msg.type = 1; 
                    msg.source_id = getpid(); // Tu ID bedzie inne niz wyswietlania, ale to bez znaczenia dla logiki
                    msg.args[0] = my_player_id;
                    int valid = 0;

                    if (strcmp(token, "TRAIN") == 0) {
                        char *arg1 = strtok(NULL, " ");
                        char *arg2 = strtok(NULL, " ");
                        if (arg1 && arg2) {
                            msg.cmd = CMD_TRAIN;
                            msg.args[1] = str_to_int(arg1);
                            msg.args[2] = str_to_int(arg2);
                            valid = 1;
                        }
                    } else if (strcmp(token, "ATTACK") == 0) {
                        char *arg1 = strtok(NULL, " ");
                        char *arg2 = strtok(NULL, " ");
                        char *arg3 = strtok(NULL, " ");
                        if (arg1 && arg2 && arg3) {
                            msg.cmd = CMD_ATTACK;
                            msg.args[1] = str_to_int(arg1);
                            msg.args[2] = str_to_int(arg2);
                            msg.args[3] = str_to_int(arg3);
                            valid = 1;
                        }
                    }

                    if (valid) {
                        msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                    }
                }
                
                pos = 0; // Reset bufora

            } else {
                // Zwykly znak - dopisz do bufora i wyswietl
                if (pos < 255) {
                    buffer[pos++] = c;
                    write(STDOUT_FILENO, &c, 1);
                }
            }
        }
    }
}

int main() {
    msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        write_str("Brak serwera\n");
        return 1;
    }

    srand(time(NULL) ^ getpid());
    int temp_key = rand() % 1000 + 50;
    
    // Logowanie
    Message msg;
    msg.type = 1;
    msg.cmd = CMD_LOGIN;
    msg.source_id = temp_key;
    msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);

    msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), temp_key, 0);
    my_player_id = msg.args[0];
    my_msg_type = 10 + my_player_id;

    // Wyczysc ekran na start
    write_str("\033[2J");

    if (fork() == 0) {
        // Dziecko: zajmuje sie tylko wyswietlaniem
        display_loop();
    } else {
        // Rodzic: zajmuje sie tylko klawiatura
        input_loop();
    }
    return 0;
}