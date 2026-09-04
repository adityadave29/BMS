#include "server.h"

// Add a new customer using binary records and atomic counter
void add_new_customer(int client_sock)
{
    char phone_number[PHONE_LEN], email[EMAIL_LEN], password[PASS_LEN], address[ADDR_LEN], account_type[TYPE_LEN];
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];

    int new_account_no = get_next_account_number();

    // Step 1: Phone number
    snprintf(send_buf, sizeof(send_buf), "Enter customer phone number: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(phone_number, recv_buf, sizeof(phone_number) - 1);

    // Step 2: Email
    snprintf(send_buf, sizeof(send_buf), "Enter customer email: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(email, recv_buf, sizeof(email) - 1);

    // Step 3: Password
    snprintf(send_buf, sizeof(send_buf), "Enter password for customer: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(password, recv_buf, sizeof(password) - 1);

    // Step 4: Address
    snprintf(send_buf, sizeof(send_buf), "Enter customer address: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(address, recv_buf, sizeof(address) - 1);

    // Step 5: Account type
    snprintf(send_buf, sizeof(send_buf), "Enter account type (savings/current): ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(account_type, recv_buf, sizeof(account_type) - 1);

    // Step 6: Append Customer record
    pthread_mutex_lock(&customers_mutex);
    int fd_cust = open(CUSTOMERS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd_cust >= 0)
    {
        Customer c;
        memset(&c, 0, sizeof(Customer));
        c.account_no = new_account_no;
        strncpy(c.phone, phone_number, sizeof(c.phone) - 1);
        strncpy(c.email, email, sizeof(c.email) - 1);
        strncpy(c.password, password, sizeof(c.password) - 1);
        strncpy(c.address, address, sizeof(c.address) - 1);
        strncpy(c.account_type, account_type, sizeof(c.account_type) - 1);

        off_t offset = lseek(fd_cust, 0, SEEK_END);
        lock_record(fd_cust, offset, sizeof(Customer), F_WRLCK);
        write(fd_cust, &c, sizeof(Customer));
        lock_record(fd_cust, offset, sizeof(Customer), F_UNLCK);
        close(fd_cust);
    }
    pthread_mutex_unlock(&customers_mutex);

    // Step 7: Append User login record (is_active = 1, is_logged_in = 0)
    pthread_mutex_lock(&users_mutex);
    int fd_user = open(USERS_DB, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd_user >= 0)
    {
        User u;
        memset(&u, 0, sizeof(User));
        u.id = new_account_no;
        strncpy(u.email, email, sizeof(u.email) - 1);
        strncpy(u.password, password, sizeof(u.password) - 1);
        strncpy(u.role, "user", sizeof(u.role) - 1);
        u.is_active = 1;
        u.is_logged_in = 0;

        off_t offset = lseek(fd_user, 0, SEEK_END);
        lock_record(fd_user, offset, sizeof(User), F_WRLCK);
        write(fd_user, &u, sizeof(User));
        lock_record(fd_user, offset, sizeof(User), F_UNLCK);
        close(fd_user);
    }
    pthread_mutex_unlock(&users_mutex);

    // Step 8: Initialize balance record with 0.0
    update_balance(new_account_no, 0.0);

    // Step 9: Success message
    snprintf(send_buf, sizeof(send_buf),
             "\n✅ Customer added successfully!\nAccount Number: %d\nEmail: %s\nAccount Type: %s\nStatus: ACTIVE\n",
             new_account_no, email, account_type);
    send(client_sock, send_buf, strlen(send_buf), 0);
}

// Approve or reject a loan application in-place
void approve_reject_loans(int client_sock)
{
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    char decision[50], reason[REASON_LEN];

    memset(send_buf, 0, sizeof(send_buf));
    memset(recv_buf, 0, sizeof(recv_buf));

    // Step 1: Ask for loan ID
    snprintf(send_buf, sizeof(send_buf), "Enter Loan ID to approve/reject: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
    int target_loan_id = atoi(recv_buf);

    // Step 2: Ask approve or reject
    snprintf(send_buf, sizeof(send_buf), "Approve or Reject? (type 'approve' or 'reject'): ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
    strncpy(decision, recv_buf, sizeof(decision) - 1);

    if (strcasecmp(decision, "reject") == 0)
    {
        snprintf(send_buf, sizeof(send_buf), "Enter reason for rejection: ");
        send(client_sock, send_buf, strlen(send_buf), 0);
        memset(recv_buf, 0, sizeof(recv_buf));
        recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
        strncpy(reason, recv_buf, sizeof(reason) - 1);
    }
    else
    {
        strncpy(reason, "-", sizeof(reason) - 1);
    }

    pthread_mutex_lock(&loans_mutex);
    int fd = open(LOANS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&loans_mutex);
        snprintf(send_buf, sizeof(send_buf), "Error opening loan database.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    Loan l;
    off_t offset = 0;
    int found = 0, already_finalized = 0, updated = 0;
    int cust_acc = 0;
    double loan_amount = 0.0;

    while (1)
    {
        if (lock_record(fd, offset, sizeof(Loan), F_WRLCK) < 0)
            break;

        ssize_t r = read(fd, &l, sizeof(Loan));
        if (r < (ssize_t)sizeof(Loan))
        {
            lock_record(fd, offset, sizeof(Loan), F_UNLCK);
            break;
        }

        if (l.loan_id == target_loan_id)
        {
            found = 1;
            cust_acc = l.account_no;
            loan_amount = l.amount;

            if (strcasecmp(l.status, "approved") == 0 || strncasecmp(l.status, "rejected", 8) == 0)
            {
                already_finalized = 1;
                lock_record(fd, offset, sizeof(Loan), F_UNLCK);
                break;
            }

            if (strcasecmp(decision, "approve") == 0)
            {
                strncpy(l.status, "approved", sizeof(l.status) - 1);
                updated = 1;
            }
            else if (strcasecmp(decision, "reject") == 0)
            {
                snprintf(l.status, sizeof(l.status), "rejected-%s", reason);
                strncpy(l.reason, reason, sizeof(l.reason) - 1);
                updated = 1;
            }

            lseek(fd, offset, SEEK_SET);
            write(fd, &l, sizeof(Loan));
            lock_record(fd, offset, sizeof(Loan), F_UNLCK);
            break;
        }

        lock_record(fd, offset, sizeof(Loan), F_UNLCK);
        offset += sizeof(Loan);
    }

    close(fd);
    pthread_mutex_unlock(&loans_mutex);

    if (!found)
    {
        snprintf(send_buf, sizeof(send_buf), "\n[SERVER] Loan ID %d not found in records.\n", target_loan_id);
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    if (already_finalized)
    {
        snprintf(send_buf, sizeof(send_buf), "\n[SERVER] Loan ID %d is already finalized.\n", target_loan_id);
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    // If approved, update customer balance directly in-place
    if (updated && strcasecmp(decision, "approve") == 0)
    {
        double current_bal = get_balance(cust_acc);
        if (current_bal < 0.0)
            current_bal = 0.0;
        update_balance(cust_acc, current_bal + loan_amount);
        snprintf(send_buf, sizeof(send_buf),
                 "\n[SERVER] Loan ID %d APPROVED successfully! %.2f credited to Account %d.\n",
                 target_loan_id, loan_amount, cust_acc);
    }
    else if (updated)
    {
        snprintf(send_buf, sizeof(send_buf),
                 "\n[SERVER] Loan ID %d REJECTED. Reason: %s\n", target_loan_id, reason);
    }

    send(client_sock, send_buf, strlen(send_buf), 0);
}

// View pending loans assigned to the logged-in employee
void view_assigned_loans(int client_sock, char *email)
{
    char send_buf[BUF_SIZE];

    // Find employee ID from EMPLOYEES_DB
    pthread_mutex_lock(&employees_mutex);
    int emp_fd = open(EMPLOYEES_DB, O_RDONLY);
    if (emp_fd < 0)
    {
        pthread_mutex_unlock(&employees_mutex);
        snprintf(send_buf, sizeof(send_buf), "Error: Could not open employee database.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    Employee emp;
    off_t offset = 0;
    int emp_id = -1;

    while (1)
    {
        lock_record(emp_fd, offset, sizeof(Employee), F_RDLCK);
        ssize_t r = read(emp_fd, &emp, sizeof(Employee));
        lock_record(emp_fd, offset, sizeof(Employee), F_UNLCK);

        if (r < (ssize_t)sizeof(Employee))
            break;

        if (strcmp(emp.email, email) == 0)
        {
            emp_id = emp.emp_id;
            break;
        }
        offset += sizeof(Employee);
    }
    close(emp_fd);
    pthread_mutex_unlock(&employees_mutex);

    if (emp_id == -1)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Employee not found for email: %s\n", email);
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    pthread_mutex_lock(&loans_mutex);
    int loan_fd = open(LOANS_DB, O_RDONLY);
    if (loan_fd < 0)
    {
        pthread_mutex_unlock(&loans_mutex);
        snprintf(send_buf, sizeof(send_buf), "Error: Could not open loan database.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    snprintf(send_buf, sizeof(send_buf), "\nPending Loans Assigned to You (Employee ID: %d)\n", emp_id);
    send(client_sock, send_buf, strlen(send_buf), 0);

    Loan l;
    offset = 0;
    int found = 0;

    while (1)
    {
        lock_record(loan_fd, offset, sizeof(Loan), F_RDLCK);
        ssize_t r = read(loan_fd, &l, sizeof(Loan));
        lock_record(loan_fd, offset, sizeof(Loan), F_UNLCK);

        if (r < (ssize_t)sizeof(Loan))
            break;

        if (l.assigned_to == emp_id && strcasecmp(l.status, "pending") == 0)
        {
            found = 1;
            snprintf(send_buf, sizeof(send_buf),
                     "Loan ID: %d | Customer Acc: %d | Amount: %.2f | Type: %s | Status: %s | Date: %s\n",
                     l.loan_id, l.account_no, l.amount, l.loan_type, l.status, l.timestamp);
            send(client_sock, send_buf, strlen(send_buf), 0);
        }

        offset += sizeof(Loan);
    }
    close(loan_fd);
    pthread_mutex_unlock(&loans_mutex);

    if (!found)
    {
        snprintf(send_buf, sizeof(send_buf), "No pending loans assigned to your ID (%d).\n", emp_id);
        send(client_sock, send_buf, strlen(send_buf), 0);
    }
}

// Modify customer details (employee role) with in-place record locking
void modify_customer_details_emp(int client_sock, char *email)
{
    char recv_buf[BUF_SIZE], buffer[BUF_SIZE];
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
            "7. Exit to Employee Menu\n"
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

// Employee Menu loop
void employee_menu(int client_sock, char *email)
{
    char buffer[BUF_SIZE];

    const char *menu =
        "\n--- Employee Session Started ---\n"
        "\nEmployee Menu:\n"
        "1. Add New Customer\n"
        "2. Modify Customer Details\n"
        "3. View Assigned Loan Applications\n"
        "4. Approve/Reject Loans\n"
        "5. Logout\n"
        "Choice: ";

    send(client_sock, menu, strlen(menu), 0);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            printf("Employee with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strcmp(buffer, "1") == 0)
            add_new_customer(client_sock);
        else if (strcmp(buffer, "2") == 0)
            modify_customer_details_emp(client_sock, email);
        else if (strcmp(buffer, "3") == 0)
            view_assigned_loans(client_sock, email);
        else if (strcmp(buffer, "4") == 0)
            approve_reject_loans(client_sock);
        else if (strcmp(buffer, "5") == 0)
        {
            remove_active_user(email);
            char response[BUF_SIZE];
            snprintf(response, BUF_SIZE, "Employee %s logged out successfully.\n", email);
            send(client_sock, response, strlen(response), 0);
            break;
        }
        else
        {
            send(client_sock, "Invalid option!\n", 16, 0);
        }

        send(client_sock, menu, strlen(menu), 0);
    }
}
