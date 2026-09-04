#include "server.h"

// Admin Menu implementation
void admin_menu(int client_sock, char *email)
{
    char buffer[BUF_SIZE];

    const char *menu =
        "\n--- Admin Session Started ---\n"
        "\nAdmin Menu:\n"
        "1. Add New Employee\n"
        "2. Modify Customer Details\n"
        "3. Modify Employee Details\n"
        "4. Logout\n";

    while (1)
    {
        send(client_sock, menu, strlen(menu), 0);
        send(client_sock, "Choice: ", 8, 0);

        memset(buffer, 0, sizeof(buffer));
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            printf("Admin with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strcmp(buffer, "1") == 0)
            add_new_employee(client_sock);
        else if (strcmp(buffer, "2") == 0)
            modify_customer_details(client_sock, email);
        else if (strcmp(buffer, "3") == 0)
            modify_employee_details(client_sock, email);
        else if (strcmp(buffer, "4") == 0)
        {
            remove_active_user(email);
            snprintf(buffer, BUF_SIZE, "email %s logged out successfully.\n", email);
            send(client_sock, buffer, strlen(buffer), 0);
            break;
        }
        else
        {
            send(client_sock, "Invalid option!\n", 16, 0);
        }
    }
}

// Add a new employee to employees.dat and users.dat
void add_new_employee(int client_sock)
{
    char recv_buf[BUF_SIZE];
    char name[NAME_LEN] = "", email[EMAIL_LEN] = "", password[PASS_LEN] = "";
    char phone[PHONE_LEN] = "", address[ADDR_LEN] = "", position[POS_LEN] = "", department[DEPT_LEN] = "";

    const char *prompts[] = {
        "Enter employee name: ",
        "Enter employee email: ",
        "Enter employee password: ",
        "Enter phone number: ",
        "Enter address: ",
        "Enter position: ",
        "Enter department: "};
    char *fields[] = {name, email, password, phone, address, position, department};
    int field_sizes[] = {sizeof(name), sizeof(email), sizeof(password), sizeof(phone), sizeof(address), sizeof(position), sizeof(department)};
    int num_fields = 7;

    for (int i = 0; i < num_fields; i++)
    {
        send(client_sock, prompts[i], strlen(prompts[i]), 0);
        memset(recv_buf, 0, sizeof(recv_buf));

        int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            return;

        recv_buf[strcspn(recv_buf, "\r\n")] = 0;
        strncpy(fields[i], recv_buf, field_sizes[i] - 1);
    }

    int new_emp_id = get_next_employee_id();

    // Append to EMPLOYEES_DB
    pthread_mutex_lock(&employees_mutex);
    int fd_emp = open(EMPLOYEES_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd_emp >= 0)
    {
        Employee emp;
        memset(&emp, 0, sizeof(Employee));
        emp.emp_id = new_emp_id;
        strncpy(emp.name, name, sizeof(emp.name) - 1);
        strncpy(emp.email, email, sizeof(emp.email) - 1);
        strncpy(emp.password, password, sizeof(emp.password) - 1);
        strncpy(emp.phone, phone, sizeof(emp.phone) - 1);
        strncpy(emp.address, address, sizeof(emp.address) - 1);
        strncpy(emp.position, position, sizeof(emp.position) - 1);
        strncpy(emp.department, department, sizeof(emp.department) - 1);

        off_t offset = lseek(fd_emp, 0, SEEK_END);
        lock_record(fd_emp, offset, sizeof(Employee), F_WRLCK);
        write(fd_emp, &emp, sizeof(Employee));
        lock_record(fd_emp, offset, sizeof(Employee), F_UNLCK);
        close(fd_emp);
    }
    pthread_mutex_unlock(&employees_mutex);

    // Append to USERS_DB
    pthread_mutex_lock(&users_mutex);
    int fd_user = open(USERS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd_user >= 0)
    {
        User u;
        memset(&u, 0, sizeof(User));
        u.id = new_emp_id;
        strncpy(u.email, email, sizeof(u.email) - 1);
        strncpy(u.password, password, sizeof(u.password) - 1);
        strncpy(u.role, position, sizeof(u.role) - 1);
        u.is_active = 1;
        u.is_logged_in = 0;

        off_t offset = lseek(fd_user, 0, SEEK_END);
        lock_record(fd_user, offset, sizeof(User), F_WRLCK);
        write(fd_user, &u, sizeof(User));
        lock_record(fd_user, offset, sizeof(User), F_UNLCK);
        close(fd_user);
    }
    pthread_mutex_unlock(&users_mutex);

    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg),
             "\n✅ Employee added successfully!\nEmployee ID: %d\nName: %s\nEmail: %s\nPosition: %s\nDepartment: %s\n",
             new_emp_id, name, email, position, department);
    send(client_sock, msg, strlen(msg), 0);
}

// Modify employee details in-place
void modify_employee_details(int client_sock, char *email)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int emp_id, found = 0;
    off_t emp_offset = 0;
    Employee emp;

    send(client_sock, "Enter employee ID: ", 19, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    emp_id = atoi(recv_buf);

    pthread_mutex_lock(&employees_mutex);
    int fd = open(EMPLOYEES_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&employees_mutex);
        send(client_sock, "Error opening employee database!\n", 33, 0);
        return;
    }

    while (1)
    {
        lock_record(fd, emp_offset, sizeof(Employee), F_RDLCK);
        ssize_t r = read(fd, &emp, sizeof(Employee));
        lock_record(fd, emp_offset, sizeof(Employee), F_UNLCK);

        if (r < (ssize_t)sizeof(Employee))
            break;

        if (emp.emp_id == emp_id)
        {
            found = 1;
            break;
        }
        emp_offset += sizeof(Employee);
    }
    close(fd);
    pthread_mutex_unlock(&employees_mutex);

    if (!found)
    {
        send(client_sock, "Employee not found!\n", 20, 0);
        return;
    }

    char old_email[EMAIL_LEN];
    strncpy(old_email, emp.email, sizeof(old_email) - 1);

    while (1)
    {
        const char *menu =
            "\n1. View current details\n"
            "2. Change email\n"
            "3. Change password\n"
            "4. Change phone number\n"
            "5. Change address\n"
            "6. Change position (role)\n"
            "7. Change department\n"
            "8. Exit to Admin Menu\n"
            "Choice: ";

        send(client_sock, menu, strlen(menu), 0);
        memset(recv_buf, 0, sizeof(recv_buf));
        n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            break;

        recv_buf[strcspn(recv_buf, "\r\n")] = 0;
        int choice = atoi(recv_buf);

        if (choice == 1)
        {
            snprintf(buffer, sizeof(buffer),
                     "\nID: %d\nName: %s\nEmail: %s\nPassword: %s\nPhone: %s\nAddress: %s\nPosition: %s\nDept: %s\n",
                     emp.emp_id, emp.name, emp.email, emp.password, emp.phone, emp.address, emp.position, emp.department);
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice >= 2 && choice <= 7)
        {
            const char *prompts[] = {
                "", "",
                "Enter new employee email: ",
                "Enter new employee password: ",
                "Enter phone number: ",
                "Enter address: ",
                "Enter position: ",
                "Enter department: "
            };
            send(client_sock, prompts[choice], strlen(prompts[choice]), 0);

            memset(recv_buf, 0, sizeof(recv_buf));
            n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0)
                break;
            recv_buf[strcspn(recv_buf, "\r\n")] = 0;

            if (choice == 2)
            {
                strncpy(emp.email, recv_buf, sizeof(emp.email) - 1);

                // Update email in USERS_DB in-place
                pthread_mutex_lock(&users_mutex);
                int u_fd = open(USERS_DB, O_RDWR);
                if (u_fd >= 0)
                {
                    User u;
                    off_t u_off = 0;
                    while (1)
                    {
                        lock_record(u_fd, u_off, sizeof(User), F_WRLCK);
                        if (read(u_fd, &u, sizeof(User)) < (ssize_t)sizeof(User))
                        {
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        if (strcmp(u.email, old_email) == 0)
                        {
                            strncpy(u.email, emp.email, sizeof(u.email) - 1);
                            lseek(u_fd, u_off, SEEK_SET);
                            write(u_fd, &u, sizeof(User));
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                        u_off += sizeof(User);
                    }
                    close(u_fd);
                }
                pthread_mutex_unlock(&users_mutex);
                strncpy(old_email, emp.email, sizeof(old_email) - 1);
            }
            else if (choice == 3)
            {
                strncpy(emp.password, recv_buf, sizeof(emp.password) - 1);

                // Update password in USERS_DB in-place
                pthread_mutex_lock(&users_mutex);
                int u_fd = open(USERS_DB, O_RDWR);
                if (u_fd >= 0)
                {
                    User u;
                    off_t u_off = 0;
                    while (1)
                    {
                        lock_record(u_fd, u_off, sizeof(User), F_WRLCK);
                        if (read(u_fd, &u, sizeof(User)) < (ssize_t)sizeof(User))
                        {
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        if (strcmp(u.email, emp.email) == 0)
                        {
                            strncpy(u.password, emp.password, sizeof(u.password) - 1);
                            lseek(u_fd, u_off, SEEK_SET);
                            write(u_fd, &u, sizeof(User));
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                        u_off += sizeof(User);
                    }
                    close(u_fd);
                }
                pthread_mutex_unlock(&users_mutex);
            }
            else if (choice == 4)
                strncpy(emp.phone, recv_buf, sizeof(emp.phone) - 1);
            else if (choice == 5)
                strncpy(emp.address, recv_buf, sizeof(emp.address) - 1);
            else if (choice == 6)
            {
                strncpy(emp.position, recv_buf, sizeof(emp.position) - 1);

                // Update role in USERS_DB in-place
                pthread_mutex_lock(&users_mutex);
                int u_fd = open(USERS_DB, O_RDWR);
                if (u_fd >= 0)
                {
                    User u;
                    off_t u_off = 0;
                    while (1)
                    {
                        lock_record(u_fd, u_off, sizeof(User), F_WRLCK);
                        if (read(u_fd, &u, sizeof(User)) < (ssize_t)sizeof(User))
                        {
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        if (strcmp(u.email, emp.email) == 0)
                        {
                            strncpy(u.role, emp.position, sizeof(u.role) - 1);
                            lseek(u_fd, u_off, SEEK_SET);
                            write(u_fd, &u, sizeof(User));
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                        u_off += sizeof(User);
                    }
                    close(u_fd);
                }
                pthread_mutex_unlock(&users_mutex);
            }
            else if (choice == 7)
                strncpy(emp.department, recv_buf, sizeof(emp.department) - 1);

            // Update EMPLOYEES_DB in-place
            pthread_mutex_lock(&employees_mutex);
            fd = open(EMPLOYEES_DB, O_RDWR);
            if (fd >= 0)
            {
                lock_record(fd, emp_offset, sizeof(Employee), F_WRLCK);
                lseek(fd, emp_offset, SEEK_SET);
                write(fd, &emp, sizeof(Employee));
                lock_record(fd, emp_offset, sizeof(Employee), F_UNLCK);
                close(fd);
            }
            pthread_mutex_unlock(&employees_mutex);

            snprintf(buffer, sizeof(buffer), "Update successful!\n");
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice == 8)
        {
            break;
        }
    }
}

// Modify customer details (admin role) in-place
void modify_customer_details(int client_sock, char *email)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int acc_no, found = 0;
    off_t cust_offset = 0;
    Customer c;

    send(client_sock, "Enter customer account number: ", 31, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
    acc_no = atoi(recv_buf);

    pthread_mutex_lock(&customers_mutex);
    int fd = open(CUSTOMERS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&customers_mutex);
        send(client_sock, "Error opening customer database!\n", 33, 0);
        return;
    }

    while (1)
    {
        lock_record(fd, cust_offset, sizeof(Customer), F_RDLCK);
        ssize_t r = read(fd, &c, sizeof(Customer));
        lock_record(fd, cust_offset, sizeof(Customer), F_UNLCK);

        if (r < (ssize_t)sizeof(Customer))
            break;

        if (c.account_no == acc_no)
        {
            found = 1;
            break;
        }
        cust_offset += sizeof(Customer);
    }
    close(fd);
    pthread_mutex_unlock(&customers_mutex);

    if (!found)
    {
        send(client_sock, "Account not found!\n", 19, 0);
        return;
    }

    char old_email[EMAIL_LEN];
    strncpy(old_email, c.email, sizeof(old_email) - 1);

    while (1)
    {
        const char *menu =
            "\n1. View current details\n"
            "2. Change email\n"
            "3. Change password\n"
            "4. Change phone number\n"
            "5. Change address\n"
            "6. Change account type\n"
            "7. Exit to Admin Menu\n"
            "Choice: ";

        send(client_sock, menu, strlen(menu), 0);
        memset(recv_buf, 0, sizeof(recv_buf));
        n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            break;

        recv_buf[strcspn(recv_buf, "\r\n")] = 0;
        int choice = atoi(recv_buf);

        if (choice == 1)
        {
            snprintf(buffer, sizeof(buffer),
                     "\nAccount: %d\nPhone: %s\nEmail: %s\nPassword: %s\nAddress: %s\nType: %s\n",
                     c.account_no, c.phone, c.email, c.password, c.address, c.account_type);
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice >= 2 && choice <= 6)
        {
            const char *prompts[] = {
                "", "",
                "Enter new email: ",
                "Enter new password: ",
                "Enter new phone: ",
                "Enter new address: ",
                "Enter new account type: "
            };
            send(client_sock, prompts[choice], strlen(prompts[choice]), 0);

            memset(recv_buf, 0, sizeof(recv_buf));
            n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0)
                break;
            recv_buf[strcspn(recv_buf, "\r\n")] = 0;

            if (choice == 2)
            {
                strncpy(c.email, recv_buf, sizeof(c.email) - 1);

                // Update email in USERS_DB in-place
                pthread_mutex_lock(&users_mutex);
                int u_fd = open(USERS_DB, O_RDWR);
                if (u_fd >= 0)
                {
                    User u;
                    off_t u_off = 0;
                    while (1)
                    {
                        lock_record(u_fd, u_off, sizeof(User), F_WRLCK);
                        if (read(u_fd, &u, sizeof(User)) < (ssize_t)sizeof(User))
                        {
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        if (strcmp(u.email, old_email) == 0)
                        {
                            strncpy(u.email, c.email, sizeof(u.email) - 1);
                            lseek(u_fd, u_off, SEEK_SET);
                            write(u_fd, &u, sizeof(User));
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                        u_off += sizeof(User);
                    }
                    close(u_fd);
                }
                pthread_mutex_unlock(&users_mutex);
                strncpy(old_email, c.email, sizeof(old_email) - 1);
            }
            else if (choice == 3)
            {
                strncpy(c.password, recv_buf, sizeof(c.password) - 1);

                // Update password in USERS_DB in-place
                pthread_mutex_lock(&users_mutex);
                int u_fd = open(USERS_DB, O_RDWR);
                if (u_fd >= 0)
                {
                    User u;
                    off_t u_off = 0;
                    while (1)
                    {
                        lock_record(u_fd, u_off, sizeof(User), F_WRLCK);
                        if (read(u_fd, &u, sizeof(User)) < (ssize_t)sizeof(User))
                        {
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        if (strcmp(u.email, c.email) == 0)
                        {
                            strncpy(u.password, c.password, sizeof(u.password) - 1);
                            lseek(u_fd, u_off, SEEK_SET);
                            write(u_fd, &u, sizeof(User));
                            lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                            break;
                        }
                        lock_record(u_fd, u_off, sizeof(User), F_UNLCK);
                        u_off += sizeof(User);
                    }
                    close(u_fd);
                }
                pthread_mutex_unlock(&users_mutex);
            }
            else if (choice == 4)
                strncpy(c.phone, recv_buf, sizeof(c.phone) - 1);
            else if (choice == 5)
                strncpy(c.address, recv_buf, sizeof(c.address) - 1);
            else if (choice == 6)
                strncpy(c.account_type, recv_buf, sizeof(c.account_type) - 1);

            // Update CUSTOMERS_DB in-place
            pthread_mutex_lock(&customers_mutex);
            fd = open(CUSTOMERS_DB, O_RDWR);
            if (fd >= 0)
            {
                lock_record(fd, cust_offset, sizeof(Customer), F_WRLCK);
                lseek(fd, cust_offset, SEEK_SET);
                write(fd, &c, sizeof(Customer));
                lock_record(fd, cust_offset, sizeof(Customer), F_UNLCK);
                close(fd);
            }
            pthread_mutex_unlock(&customers_mutex);

            snprintf(buffer, sizeof(buffer), "Update successful!\n");
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice == 7)
        {
            break;
        }
    }
}
