#include "common.h"
#include <termios.h>
#include <sys/select.h>

int msg_id;
int my_player_id = -1; 
int my_msg_type = -1;

// Bufory globalne
char last_server_msg[MAX_TEXT];
char input_buffer[256];
int input_pos = 0;

// Struktura do przywracania ustawien terminala
struct termios orig_termios;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_mode() {
    struct termios new_termios;

    // Pobierz obecne ustawienia
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    atexit(reset_terminal_mode);

    // Wylacz kanonicznosc (czytanie znak po znaku) i echo
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

// Funkcja rysujaca caly ekran
void redraw() {
    // Wyczysc ekran i ustaw kursor na poczatku
    printf("\033[2J\033[H"); 

    // 1. Rysuj stan gry (otrzymany od serwera)
    if (strlen(last_server_msg) > 0) {
        printf("GRACZ %d\n", my_player_id + 1);
        printf("------------------------------------------------\n");
        printf("%s\n", last_server_msg);
        printf("------------------------------------------------\n");
    } else {
        printf("Oczekiwanie na dane z serwera...\n");
    }

    // 2. Rysuj instrukcje
    printf("\nKOMENDY: TRAIN [0-3] [ilosc] | ATTACK [L] [H] [C]\n");
    printf("(0-Lekka, 1-Ciezka, 2-Jazda, 3-Robotnik)\n");
    
    // 3. Rysuj linie komend i to co uzytkownik wpisal
    printf("\n> %s", input_buffer);
    fflush(stdout);
}

// Obsluga pojedynczego wcisniecia klawisza
void handle_input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0) {
        if (c == '\n') { // Enter
            // Parsowanie i wysylanie
            Message msg;
            msg.type = 1; 
            msg.source_id = getpid();
            msg.args[0] = my_player_id;
            int valid = 0;

            char cmd_str[10];
            int a, b, c_arg;

            if (sscanf(input_buffer, "%s", cmd_str) >= 1) {
                if (strcmp(cmd_str, "TRAIN") == 0) {
                    if (sscanf(input_buffer, "%*s %d %d", &a, &b) == 2) {
                        msg.cmd = CMD_TRAIN;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        valid = 1;
                    }
                } else if (strcmp(cmd_str, "ATTACK") == 0) {
                    if (sscanf(input_buffer, "%*s %d %d %d", &a, &b, &c_arg) == 3) {
                        msg.cmd = CMD_ATTACK;
                        msg.args[1] = a; 
                        msg.args[2] = b; 
                        msg.args[3] = c_arg; 
                        valid = 1;
                    }
                }
            }

            if (valid) {
                msgsnd(msg_id, &msg, sizeof(Message)-sizeof(long), 0);
            }

            // Wyczysc bufor po wyslaniu
            memset(input_buffer, 0, 256);
            input_pos = 0;
            redraw();

        } else if (c == 127 || c == 8) { // Backspace
            if (input_pos > 0) {
                input_buffer[--input_pos] = '\0';
                redraw();
            }
        } else if (c >= 32 && c <= 126) { // Znaki drukowalne
            if (input_pos < 255) {
                input_buffer[input_pos++] = c;
                input_buffer[input_pos] = '\0';
                redraw();
            }
        }
    }
}

int main() {
    msg_id = msgget(KEY_MSG, 0666);
    if (msg_id == -1) {
        perror("Brak serwera");
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

    // Przelacz terminal w tryb "RAW" (bez Entera, bez echo)
    set_conio_mode();
    memset(last_server_msg, 0, MAX_TEXT);
    memset(input_buffer, 0, 256);

    redraw();

    while(1) {
        // 1. Sprawdz czy jest wiadomosc od serwera (IPC_NOWAIT nie blokuje)
        if (msgrcv(msg_id, &msg, sizeof(Message)-sizeof(long), my_msg_type, IPC_NOWAIT) != -1) {
            if (msg.cmd == CMD_UPDATE) {
                strcpy(last_server_msg, msg.text);
                redraw();
            } else if (msg.cmd == CMD_RESULT) {
                reset_terminal_mode(); // Przywroc terminal przed wyjsciem
                printf("\033[2J\033[H");
                printf("\n================================\n");
                printf("KONIEC GRY: %s\n", msg.text);
                printf("================================\n");
                exit(0);
            }
        }

        // 2. Sprawdz czy uzytkownik cos wcisnal (uzywajac select)
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms timeout

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (ret > 0) {
            handle_input();
        }

        // Krotka pauza zeby nie obciazac CPU
        usleep(10000); 
    }
    return 0;
}