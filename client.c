#include "common.h"
#include <termios.h>

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

// --- Funkcje pomocnicze (bo brak sprintf/scanf) ---

void write_str(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

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

int str_to_int(char* str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

// Konfiguracja terminala (wylaczenie Echa systemowego)
struct termios orig_termios;

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    write_str("\033[?25h"); // Pokaz kursor na koniec
}

void set_conio_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode);
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}


// --- PROCES DZIECKA: Tylko wyświetlanie ---
// Ten proces zajmuje się tylko liniami 1-10. Nie tyka linii 12+ gdzie piszesz.

void display_loop() {
    Message msg;
    char out_buf[4096];
    
    while(1) {
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, 0) == -1) {
            exit(0);
        }

        if (msg.cmd == CMD_UPDATE) {
            // \033[s - ZAPISZ pozycje kursora (tam gdzie rodzic pisze na dole)
            // \033[?25l - ukryj kursor zeby nie migal na gorze
            out_buf[0] = '\0';
            strcat(out_buf, "\033[s\033[?25l"); 
            
            // Przesun na gore (1,1)
            strcat(out_buf, "\033[1;1H"); 

            // Rysuj statystyki
            strcat(out_buf, "=== GRACZ ");
            append_int(out_buf, my_player_id + 1);
            strcat(out_buf, " ===\033[K\n");

            char *line = strtok(msg.text, "\n");
            while(line != NULL) {
                strcat(out_buf, line);
                strcat(out_buf, "\033[K\n"); // \033[K czysci reszte linii ze starych cyferek
                line = strtok(NULL, "\n");
            }

            strcat(out_buf, "------------------------------------------------\033[K\n");
            strcat(out_buf, "KOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\033[K\n");
            strcat(out_buf, "(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\033[K");

            // \033[u - PRZYWROC kursor na dol (tam gdzie pisales)
            // \033[?25h - Pokaz kursor
            strcat(out_buf, "\033[u\033[?25h");

            // Wykonaj calosc w jednym 'write' zeby bylo atomowo
            write_str(out_buf);

        } else if (msg.cmd == CMD_RESULT) {
            out_buf[0] = '\0';
            strcat(out_buf, "\033[2J\033[H\n=== KONIEC GRY ===\n");
            strcat(out_buf, msg.text);
            strcat(out_buf, "\n\n");
            write_str(out_buf);
            kill(getppid(), SIGKILL); // Zabij proces rodzica
            exit(0);
        }
    }
}

// --- PROCES RODZICA: Tylko klawiatura ---
// Ten proces siedzi w linii 12 i tylko tam pisze.

void input_loop() {
    char c;
    char buffer[256];
    int pos = 0;

    // Ustaw kursor poczatkowy w linii 12
    write_str("\033[12;1H> ");

    while(1) {
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') { // ENTER
                // Wyczysc linie komendy wizualnie (\r - powrot, \033[K - czysc)
                write_str("\r\033[K> "); 
                
                buffer[pos] = '\0';
                
                // Parsowanie (recznie bez sscanf)
                char temp_buf[256];
                strcpy(temp_buf, buffer);
                
                char *token = strtok(temp_buf, " ");
                if (token != NULL) {
                    Message msg;
                    msg.type = 1; 
                    msg.source_id = getpid();
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
                    if (valid) msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                }
                pos = 0; // Reset bufora

            } else if (c == 127 || c == 8) { // Backspace
                if (pos > 0) {
                    pos--;
                    write_str("\b \b"); // Cofnij, zmaz spacja, cofnij
                }
            } else { // Zwykly znak
                if (pos < 250) {
                    buffer[pos++] = c;
                    write(STDOUT_FILENO, &c, 1); // Echo na ekran (bo systemowe wylaczone)
                }
            }
        }
    }
}

int main() {
    msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        write_str("Brak serwera. Uruchom najpierw ./server\n");
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

    // Przygotuj ekran
    write_str("\033[2J"); // Wyczysc wszystko raz
    
    // Wylacz systemowe echo (zeby nasze write nie gryzlo sie z systemem)
    set_conio_mode();

    if (fork() == 0) {
        display_loop();
    } else {
        input_loop();
    }
    return 0;
}