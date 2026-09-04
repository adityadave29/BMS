#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 9090
#define BUF_SIZE 1024

int sock_global;
char email_global[100] = "";

void login_user(int sock);
void exit_session(int sock);
void client_menu(int sock, const char *initial_buf);
void signal_handler(int sig);

// Checks if the server's output buffer ends in an input prompt (e.g. "Choice: ", "Enter email: ", "? ")
static int is_server_prompt(const char *buf)
{
    int len = strlen(buf);
    if (len == 0)
        return 0;

    // Trim trailing whitespace
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t' || buf[len - 1] == '\r' || buf[len - 1] == '\n'))
        len--;

    if (len > 0 && (buf[len - 1] == ':' || buf[len - 1] == '?'))
        return 1;

    return 0;
}

// Session communication loop for all roles (Customer, Admin, Employee, Manager)
void client_menu(int sock, const char *initial_buf)
{
    char receiver_buffer[BUF_SIZE * 4];
    char input_buffer[BUF_SIZE];
    int n;

    // If the login handshake response already contained the menu prompt
    if (initial_buf != NULL && strlen(initial_buf) > 0)
    {
        if (is_server_prompt(initial_buf))
        {
            memset(input_buffer, 0, sizeof(input_buffer));
            if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
            {
                input_buffer[strcspn(input_buffer, "\r\n")] = '\0';
                send(sock, input_buffer, strlen(input_buffer), 0);
            }
        }
    }

    while (1)
    {
        memset(receiver_buffer, 0, sizeof(receiver_buffer));
        n = recv(sock, receiver_buffer, sizeof(receiver_buffer) - 1, 0);
        if (n <= 0)
        {
            printf("\nDisconnected from server.\n");
            break;
        }

        receiver_buffer[n] = '\0';
        printf("%s", receiver_buffer);
        fflush(stdout);

        // Check if session ended via logout or exit
        if (strstr(receiver_buffer, "logged out successfully") || strstr(receiver_buffer, "exited the application"))
        {
            printf("\n--- Session Ended ---\n\n");
            break;
        }

        // If the server sent a prompt requiring input, read from user and send back
        if (is_server_prompt(receiver_buffer))
        {
            memset(input_buffer, 0, sizeof(input_buffer));
            if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
                break;
            input_buffer[strcspn(input_buffer, "\r\n")] = '\0';

            // Send user input to server
            send(sock, input_buffer, strlen(input_buffer), 0);
        }
    }
}

// Handles initial login handshake
void login_user(int sock)
{
    char email[100], password[100], sender_buffer[BUF_SIZE], receiver_buffer[BUF_SIZE], role[50];

    printf("Enter email: ");
    scanf("%99s", email);
    printf("Enter password: ");
    scanf("%99s", password);
    printf("Enter role (User/employee/manager/admin): ");
    scanf("%49s", role);

    int c;
    while ((c = getchar()) != '\n' && c != EOF); // Flush trailing newline from stdin

    snprintf(sender_buffer, BUF_SIZE, "login %s %s %s", email, password, role);
    send(sock, sender_buffer, strlen(sender_buffer), 0);

    memset(receiver_buffer, 0, BUF_SIZE);
    int n = recv(sock, receiver_buffer, BUF_SIZE - 1, 0);
    if (n <= 0)
    {
        printf("No response from server.\n");
        return;
    }
    receiver_buffer[n] = '\0';

    printf("Server: %s\n", receiver_buffer);
    fflush(stdout);

    if (strstr(receiver_buffer, "Login successful"))
    {
        strncpy(email_global, email, sizeof(email_global) - 1);
        client_menu(sock, receiver_buffer);
    }
}

// Graceful signal handler (Ctrl+C / Ctrl+Z)
void signal_handler(int sig)
{
    printf("\nSignal %d received. Closing session...\n", sig);
    signal(sig, SIG_IGN);

    if (strlen(email_global) > 0)
    {
        char buffer[BUF_SIZE];
        snprintf(buffer, sizeof(buffer), "disconnect %s", email_global);
        send(sock_global, buffer, strlen(buffer), 0);
        usleep(100000);
    }

    close(sock_global);
    exit(0);
}

// Handles exit session
void exit_session(int sock)
{
    send(sock, "exit", 4, 0);
    printf("Exiting...\n");
    close(sock);
    exit(0);
}

// Main application loop
int main()
{
    int sock;
    struct sockaddr_in server_addr;

    signal(SIGINT, signal_handler);  // Ctrl+C
    signal(SIGTSTP, signal_handler); // Ctrl+Z

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket error");
        exit(1);
    }
    sock_global = sock;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to server on port %d.\n", PORT);

    while (1)
    {
        int choice = 0;
        char choice_buf[32];
        printf("\nMenu:\n1. Login\n2. Exit\nChoice: ");
        if (fgets(choice_buf, sizeof(choice_buf), stdin) == NULL)
            break;
        choice = atoi(choice_buf);

        switch (choice)
        {
        case 1:
            login_user(sock);
            break;
        case 2:
            exit_session(sock);
            break;
        default:
            printf("Invalid choice!\n");
        }
    }

    close(sock);
    return 0;
}