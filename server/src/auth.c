#include "server.h"

// Main login function using fixed-size binary records and fcntl record locking
void login_user(int client_sock, char *email, char *password, char *role)
{
    pthread_mutex_lock(&users_mutex);
    int fd = open(USERS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        send(client_sock, "Database error\n", 15, 0);
        return;
    }

    User u;
    off_t offset = 0;
    int found = 0;
    int deactivated = 0;
    int already_logged_in = 0;

    while (1)
    {
        // Lock this specific User record for reading
        if (lock_record(fd, offset, sizeof(User), F_RDLCK) < 0)
            break;

        ssize_t r = read(fd, &u, sizeof(User));
        if (r < (ssize_t)sizeof(User))
        {
            lock_record(fd, offset, sizeof(User), F_UNLCK);
            break;
        }

        // Case-insensitive role comparison and exact email/pass comparison
        if (strcmp(u.email, email) == 0 &&
            strcmp(u.password, password) == 0 &&
            strcasecmp(u.role, role) == 0)
        {
            found = 1;

            if (u.is_active == 0)
            {
                deactivated = 1;
                lock_record(fd, offset, sizeof(User), F_UNLCK);
                break;
            }

            if (u.is_logged_in == 1)
            {
                already_logged_in = 1;
                lock_record(fd, offset, sizeof(User), F_UNLCK);
                break;
            }

            // Upgrade to write lock to mark user as logged in
            lock_record(fd, offset, sizeof(User), F_UNLCK);
            lock_record(fd, offset, sizeof(User), F_WRLCK);

            u.is_logged_in = 1;
            lseek(fd, offset, SEEK_SET);
            write(fd, &u, sizeof(User));

            // Release lock
            lock_record(fd, offset, sizeof(User), F_UNLCK);
            break;
        }

        lock_record(fd, offset, sizeof(User), F_UNLCK);
        offset += sizeof(User);
    }

    close(fd);
    pthread_mutex_unlock(&users_mutex);

    if (!found)
    {
        send(client_sock, "Invalid username/password!\n", 27, 0);
        return;
    }

    if (deactivated)
    {
        send(client_sock, "Your account is deactivated. Contact admin.\n", 44, 0);
        return;
    }

    if (already_logged_in)
    {
        send(client_sock, "Already logged in from another device!\n", 39, 0);
        return;
    }

    send(client_sock, "Login successful!\n", 18, 0);

    if (strcasecmp(role, "user") == 0)
        client_menu(client_sock, email);
    else if (strcasecmp(role, "admin") == 0)
        admin_menu(client_sock, email);
    else if (strcasecmp(role, "employee") == 0)
        employee_menu(client_sock, email);
    else if (strcasecmp(role, "manager") == 0)
        manager_menu(client_sock, email);
    else
    {
        send(client_sock, "Unknown role. Access denied.\n", 30, 0);
        set_user_login_status(email, 0);
    }
}

// Exit session
void exit_session(int client_sock)
{
    send(client_sock, "Exit!\n", 6, 0);
}
