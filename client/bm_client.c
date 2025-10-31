#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 1024

int sock_global;
char email_global[100] = "";

void login_user(int sock);
void exit_session(int sock);
void client_menu(int sock);
void signal_handler(int sig);

// This function will take input from client and send to server for login
void login_user(int sock)
{
    char email[100], password[100], sender_buffer[BUF_SIZE], receiver_buffer[BUF_SIZE], role[50];

    printf("Enter email: ");
    scanf("%s", email);
    printf("Enter password: ");
    scanf("%s", password);
    printf("Enter role(User/employee/manager): ");
    scanf("%s", role);

    snprintf(sender_buffer, BUF_SIZE, "login %s %s %s", email, password, role);
    send(sock, sender_buffer, strlen(sender_buffer), 0);

    memset(receiver_buffer, 0, BUF_SIZE);
    recv(sock, receiver_buffer, BUF_SIZE, 0);
    printf("Server: %s\n", receiver_buffer);

    // here we are taking feedback from server about login status
    if (strstr(receiver_buffer, "Login successful"))
    {
        strcpy(email_global, email);
        client_menu(sock);
    }
}

// signal handler when user presses Ctrl+C or Ctrl+Z
void signal_handler(int sig)
{
    printf("\nSignal %d received. Waiting 2 seconds to finish any ongoing operations...\n", sig);

    signal(sig, SIG_IGN);
    sleep(2);

    if (strlen(email_global) > 0)
    {
        char buffer[BUF_SIZE];
        snprintf(buffer, sizeof(buffer), "disconnect %s", email_global);
        send(sock_global, buffer, strlen(buffer), 0);
        usleep(200000);
    }

    printf("\nSession closed (signal %d). Exiting...\n", sig);

    close(sock_global);
    exit(0);
}

// here based on need this function will take input from client
void client_menu(int sock)
{
    char sender_buffer[BUF_SIZE], receiver_buffer[BUF_SIZE * 4];
    int n;

    memset(receiver_buffer, 0, sizeof(receiver_buffer));
    n = recv(sock, receiver_buffer, sizeof(receiver_buffer) - 1, 0);
    if (n > 0)
    {
        receiver_buffer[n] = '\0';
        printf("%s", receiver_buffer);
    }

    while (1)
    {
        printf("Enter choice: ");
        fgets(sender_buffer, sizeof(sender_buffer), stdin);
        sender_buffer[strcspn(sender_buffer, "\n")] = 0;

        if (strlen(sender_buffer) == 0)
            continue;

        send(sock, sender_buffer, strlen(sender_buffer), 0);

        memset(receiver_buffer, 0, sizeof(receiver_buffer));
        int total = 0;

        while ((n = recv(sock, receiver_buffer + total, sizeof(receiver_buffer) - 1 - total, 0)) > 0)
        {
            total += n;
            receiver_buffer[total] = '\0';

            if (
                // ---- Customer Operations ----
                strstr(receiver_buffer, "Enter amount to deposit:") ||
                strstr(receiver_buffer, "Enter amount to withdraw:") ||
                strstr(receiver_buffer, "Enter recipient account number:") ||
                strstr(receiver_buffer, "Enter amount to transfer:") ||
                strstr(receiver_buffer, "Enter loan amount:") ||
                strstr(receiver_buffer, "Enter loan type") ||
                strstr(receiver_buffer, "Enter your feedback:") ||
                strstr(receiver_buffer, "Enter old password:") ||
                strstr(receiver_buffer, "Enter new password:") ||
                strstr(receiver_buffer, "Confirm new password:") ||
                strstr(receiver_buffer, "Enter your new password:") ||

                // ---- Employee creation/modification ----
                strstr(receiver_buffer, "Enter employee name:") ||
                strstr(receiver_buffer, "Enter employee email:") ||
                strstr(receiver_buffer, "Enter employee password:") ||
                strstr(receiver_buffer, "Enter employee phone number:") ||
                strstr(receiver_buffer, "Enter phone number:") ||
                strstr(receiver_buffer, "Enter address:") ||
                strstr(receiver_buffer, "Enter position:") ||
                strstr(receiver_buffer, "Enter department:") ||
                strstr(receiver_buffer, "Enter Loan ID to assign:") ||
                strstr(receiver_buffer, "Enter Employee ID to assign to:") ||
                strstr(receiver_buffer, "Enter Loan ID to approve/reject:") ||
                strstr(receiver_buffer, "Approve or Reject?") ||
                strstr(receiver_buffer, "Enter reason for rejection:") ||

                // ---- Customer Management (Employee Side) ----
                strstr(receiver_buffer, "Enter customer phone number:") ||
                strstr(receiver_buffer, "Enter customer email:") ||
                strstr(receiver_buffer, "Enter password for customer:") ||
                strstr(receiver_buffer, "Enter customer address:") ||
                strstr(receiver_buffer, "Enter account type (savings/current):") ||

                // ---- Modify Customer ----
                strstr(receiver_buffer, "Enter customer account number:") ||
                strstr(receiver_buffer, "Enter new email:") ||
                strstr(receiver_buffer, "Enter new password:") ||
                strstr(receiver_buffer, "Enter new phone:") ||
                strstr(receiver_buffer, "Enter new address:") ||
                strstr(receiver_buffer, "Enter new account type:") ||

                // ---- Modify Employee ----
                strstr(receiver_buffer, "Enter employee ID:") ||
                strstr(receiver_buffer, "Enter new position:") ||
                strstr(receiver_buffer, "Enter new department:") ||
                strstr(receiver_buffer, "Enter user email to modify:") ||
                strstr(receiver_buffer, "Enter 1 to activate or 0 to deactivate:"))
            {
                printf("%s", receiver_buffer);
                char input[BUF_SIZE];
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = 0;
                send(sock, input, strlen(input), 0);

                total = 0;
                memset(receiver_buffer, 0, sizeof(receiver_buffer));
                continue;
            }
            if (strstr(receiver_buffer, "Choice:"))
                break;
        }

        if (n <= 0)
            break;

        printf("%s", receiver_buffer);

        if (strstr(receiver_buffer, "logged out successfully") || strstr(receiver_buffer, "exited the application"))
        {
            printf("\n--- Session Ended ---\n\n");
            break;
        }
    }
}

// this will handle exit session
void exit_session(int sock)
{
    send(sock, "exit", 4, 0);
    printf("Exiting...\n");
    close(sock);
    exit(0);
}

// Here we will make connection to server and handle signals
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

    printf("Connected to server.\n");

    while (1)
    {
        int choice;
        printf("\nMenu:\n1. Login\n2. Exit\nChoice: ");
        scanf("%d", &choice);

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