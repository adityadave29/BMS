#include "server.h"

// checking if user is already logged in from another device
int is_user_active(const char *email, const char *password, const char *role)
{
    FILE *fp = fopen(ACTIVE_USERS, "r");
    if (!fp)
        return 0;

    char a_email[100], a_pass[100], a_role[50];
    while (fscanf(fp, "%s %s %s", a_email, a_pass, a_role) != EOF)
    {
        if (strcmp(a_email, email) == 0 &&
            strcmp(a_pass, password) == 0 &&
            strcmp(a_role, role) == 0)
        {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0; // Not active
}

// if not already logged in then add to active users file and allow them to log in
void add_active_user(const char *email, const char *password, const char *role)
{
    FILE *fp = fopen(ACTIVE_USERS, "a");
    if (!fp)
        return;
    fprintf(fp, "%s %s %s\n", email, password, role);
    fclose(fp);
}

// Here main login function
void login_user(int client_sock, char *email, char *password, char *role)
{
    FILE *fp = fopen(USERS, "r");
    if (!fp)
    {
        send(client_sock, "Database error\n", 15, 0);
        return;
    }

    char line[256];
    fgets(line, sizeof(line), fp);

    // Check if already active before proceeding
    if (is_user_active(email, password, role))
    {
        send(client_sock, "Already logged in from another device!\n", 39, 0);
        fclose(fp);
        return;
    }

    char file_user[100], file_pass[100], file_role[50];
    int active;
    int found = 0;

    while (fscanf(fp, "%99s %99s %49s %d", file_user, file_pass, file_role, &active) == 4)
    {
        if (strcmp(file_user, email) == 0 &&
            strcmp(file_pass, password) == 0 &&
            strcmp(file_role, role) == 0)
        {
            found = 1;
            if (active == 0)
            {
                send(client_sock, "Your account is deactivated. Contact admin.\n", 44, 0);
                fclose(fp);
                return;
            }
            break;
        }
    }
    fclose(fp);

    if (found)
    {
        send(client_sock, "Login successful!\n", 18, 0);

        add_active_user(email, password, role);

        if (strcmp(role, "user") == 0)
            client_menu(client_sock, email);
        else if (strcmp(role, "admin") == 0)
            admin_menu(client_sock, email);
        else if (strcmp(role, "employee") == 0)
            employee_menu(client_sock, email);
        else if (strcmp(role, "manager") == 0)
            manager_menu(client_sock, email);
        else
        {
            send(client_sock, "Unknown role. Access denied.\n", 30, 0);
        }
    }
    else
    {
        send(client_sock, "Invalid username/password!\n", 27, 0);
    }
}

// this will handle exit session
void exit_session(int client_sock)
{
    send(client_sock, "Exit!\n", 9, 0);
    close(client_sock);
}
