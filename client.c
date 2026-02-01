#include "common.h"

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

// Funkcja pomocnicza: write string
void write_str(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

// Funkcja pomocnicza: int na string
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

// Funkcja pomocnicza: string na int
int str_to_int(char* str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

// PROCES 1: Odpowiedzialny TYLKO za wyswietlanie
void display_loop() {
    Message msg;
    char out_buf[4096];
    char line_buf[512];

    while(1) {
        // Czekaj na wiadomosc od serwera (funkcja blokujaca)
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, 0) == -1) {
            exit(0);
        }

        if (msg.cmd == CMD_UPDATE) {
            // Budujemy caly ekran w buforze
            out_buf[0] = '\0';
            
            // Czyszczenie ekranu (ANSI Escape Code: Clear Screen + Home Cursor)
            strcat(out_buf, "\033[2J\033[H");
            
            strcat(out_buf, "=== GRACZ ");
            append_int(out_buf, my_player_id + 1);
            strcat(out_buf, " ===\n\n");

            strcat(out_buf, msg.text);
            strcat(out_buf, "\n\n");
            
            strcat(out_buf, "------------------------------------------------\n");
            strcat(out_buf, "KOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\n");
            strcat(out_buf, "(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\n");
            strcat(out_buf, "------------------------------------------------\n");
            strcat(out_buf, "WPISZ KOMENDE:\n"); // Zacheta, ale kursor bedzie gdzie indziej

            // Jedno wywolanie write, zeby nie migalo
            write_str(out_buf);

        } else if (msg.cmd == CMD_RESULT) {
            out_buf[0] = '\0';
            strcat(out_buf, "\033[2J\033[H\n=== KONIEC GRY ===\n\n");
            strcat(out_buf, msg.text);
            strcat(out_buf, "\n\n");
            write_str(out_buf);
            kill(getppid(), SIGKILL); // Zabijamy proces klawiatury
            exit(0);
        }
    }
}

// PROCES 2: Odpowiedzialny TYLKO za klawiature
void input_loop() {
    char buffer[256];
    char c;
    int pos = 0;

    while(1) {
        // Czytamy znak po znaku ze standardowego wejscia
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') {
                // Enter - parsujemy komende
                buffer[pos] = '\0';
                
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

                    if (valid) {
                        msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
                    }
                }
                pos = 0; // Reset bufora

            } else {
                // Zapisujemy znak do bufora
                if (pos < 255) {
                    buffer[pos++] = c;
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

    if (fork() == 0) {
        display_loop();
    } else {
        input_loop();
    }
    return 0;
}