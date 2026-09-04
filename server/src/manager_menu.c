#include "server.h"

// Activate or Deactivate Customer Account in-place in users.dat
void activate_deactivate_customer(int client_sock)
{
    char msg[256], recv_buf[256];
    char email[EMAIL_LEN];
    int status_found = 0;

    // Step 1: Ask for customer email
    snprintf(msg, sizeof(msg), "Enter customer email: ");
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(email, recv_buf, sizeof(email) - 1);

    // Step 2: Ask for action (1 = Activate, 0 = Deactivate)
    snprintf(msg, sizeof(msg), "Enter 1 to activate or 0 to deactivate: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    int activation = atoi(recv_buf);
    if (activation != 0 && activation != 1)
    {
        snprintf(msg, sizeof(msg), "Invalid choice! Please enter 1 or 0 only.\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    pthread_mutex_lock(&users_mutex);
    int fd = open(USERS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        snprintf(msg, sizeof(msg), "Error opening users database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    User u;
    off_t offset = 0;
    while (1)
    {
        if (lock_record(fd, offset, sizeof(User), F_WRLCK) < 0)
            break;

        ssize_t r = read(fd, &u, sizeof(User));
        if (r < (ssize_t)sizeof(User))
        {
            lock_record(fd, offset, sizeof(User), F_UNLCK);
            break;
        }

        if (strcmp(u.email, email) == 0)
        {
            status_found = 1;
            u.is_active = activation;
            // If deactivating, force logout
            if (activation == 0)
                u.is_logged_in = 0;

            lseek(fd, offset, SEEK_SET);
            write(fd, &u, sizeof(User));
            lock_record(fd, offset, sizeof(User), F_UNLCK);
            break;
        }

        lock_record(fd, offset, sizeof(User), F_UNLCK);
        offset += sizeof(User);
    }
    close(fd);
    pthread_mutex_unlock(&users_mutex);

    if (status_found)
    {
        if (activation == 1)
            snprintf(msg, sizeof(msg), "Customer account '%s' ACTIVATED successfully.\n", email);
        else
            snprintf(msg, sizeof(msg), "Customer account '%s' DEACTIVATED successfully.\n", email);
    }
    else
    {
        snprintf(msg, sizeof(msg), "Customer email '%s' not found in records!\n", email);
    }

    send(client_sock, msg, strlen(msg), 0);
}

// List all employees from employees.dat
void list_all_employees(int client_sock)
{
    pthread_mutex_lock(&employees_mutex);
    int fd = open(EMPLOYEES_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&employees_mutex);
        send(client_sock, "Error opening employee database!\n", 33, 0);
        return;
    }

    char header[BUF_SIZE];
    snprintf(header, BUF_SIZE, "\n%-5s %-15s %-25s %-15s %-15s %-15s %-10s\n",
             "ID", "NAME", "EMAIL", "PHONE", "ADDRESS", "POSITION", "DEPT");
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "------------------------------------------------------------------------------------------------------\n", 102, 0);

    Employee emp;
    off_t offset = 0;

    while (1)
    {
        lock_record(fd, offset, sizeof(Employee), F_RDLCK);
        ssize_t r = read(fd, &emp, sizeof(Employee));
        lock_record(fd, offset, sizeof(Employee), F_UNLCK);

        if (r < (ssize_t)sizeof(Employee))
            break;

        char buffer[BUF_SIZE];
        snprintf(buffer, BUF_SIZE, "%-5d %-15s %-25s %-15s %-15s %-15s %-10s\n",
                 emp.emp_id, emp.name, emp.email, emp.phone, emp.address, emp.position, emp.department);
        send(client_sock, buffer, strlen(buffer), 0);

        offset += sizeof(Employee);
    }

    close(fd);
    pthread_mutex_unlock(&employees_mutex);
}

// List unassigned loan applications from loans.dat
void list_unassigned_loans(int client_sock)
{
    pthread_mutex_lock(&loans_mutex);
    int fd = open(LOANS_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&loans_mutex);
        send(client_sock, "Error opening loan database!\n", 29, 0);
        return;
    }

    char header[BUF_SIZE];
    snprintf(header, BUF_SIZE, "\n%-15s %-10s %-12s %-10s %-10s %-15s %-20s\n",
             "ACCOUNT_NUMBER", "LOAN_ID", "AMOUNT", "TYPE", "STATUS", "ASSIGNED_TO", "TIMESTAMP");
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "---------------------------------------------------------------------------------------------\n", 93, 0);

    Loan l;
    off_t offset = 0;
    int found = 0;

    while (1)
    {
        lock_record(fd, offset, sizeof(Loan), F_RDLCK);
        ssize_t r = read(fd, &l, sizeof(Loan));
        lock_record(fd, offset, sizeof(Loan), F_UNLCK);

        if (r < (ssize_t)sizeof(Loan))
            break;

        if (l.assigned_to == 0)
        {
            found = 1;
            char buffer[BUF_SIZE];
            snprintf(buffer, BUF_SIZE, "%-15d %-10d %-12.2f %-10s %-10s %-15d %-20s\n",
                     l.account_no, l.loan_id, l.amount, l.loan_type, l.status, l.assigned_to, l.timestamp);
            send(client_sock, buffer, strlen(buffer), 0);
        }

        offset += sizeof(Loan);
    }

    close(fd);
    pthread_mutex_unlock(&loans_mutex);

    if (!found)
    {
        send(client_sock, "No unassigned loans found.\n", 27, 0);
    }
}

// Assign loan application to employee in-place
void assign_loan_to_employee(int client_sock)
{
    char recv_buf[BUF_SIZE];
    int loan_id, emp_id, found = 0;

    // Step 1: Ask for Loan ID
    send(client_sock, "Enter Loan ID to assign: ", 25, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
    loan_id = atoi(recv_buf);

    // Step 2: Ask for Employee ID
    send(client_sock, "Enter Employee ID to assign to: ", 32, 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
    emp_id = atoi(recv_buf);

    pthread_mutex_lock(&loans_mutex);
    int fd = open(LOANS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&loans_mutex);
        send(client_sock, "Error opening loan database!\n", 29, 0);
        return;
    }

    Loan l;
    off_t offset = 0;

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

        if (l.loan_id == loan_id)
        {
            found = 1;
            l.assigned_to = emp_id;
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

    if (found)
        send(client_sock, "Loan assigned successfully!\n", 28, 0);
    else
        send(client_sock, "Loan ID not found!\n", 20, 0);
}

// Review customer feedback from feedbacks.dat
void review_customer_feedback(int client_sock)
{
    pthread_mutex_lock(&feedbacks_mutex);
    int fd = open(FEEDBACKS_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&feedbacks_mutex);
        send(client_sock, "No feedback available!\n", 23, 0);
        return;
    }

    char header[BUF_SIZE];
    snprintf(header, BUF_SIZE, "\n%-12s %-15s %-30s %-20s\n",
             "FEEDBACK_ID", "ACCOUNT_NUMBER", "FEEDBACK", "TIMESTAMP");
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "--------------------------------------------------------------------------------\n", 81, 0);

    Feedback fb;
    off_t offset = 0;
    int count = 0;

    while (1)
    {
        lock_record(fd, offset, sizeof(Feedback), F_RDLCK);
        ssize_t r = read(fd, &fb, sizeof(Feedback));
        lock_record(fd, offset, sizeof(Feedback), F_UNLCK);

        if (r < (ssize_t)sizeof(Feedback))
            break;

        count++;
        char buffer[BUF_SIZE];
        snprintf(buffer, BUF_SIZE, "%-12d %-15d %-30s %-20s\n",
                 fb.feedback_id, fb.account_no, fb.feedback, fb.timestamp);
        send(client_sock, buffer, strlen(buffer), 0);

        offset += sizeof(Feedback);
    }

    close(fd);
    pthread_mutex_unlock(&feedbacks_mutex);

    if (count == 0)
    {
        send(client_sock, "No feedback available!\n", 23, 0);
    }
}

// Change manager's own password in-place
void change_manager_own_password(int client_sock, char *email)
{
    char msg[256], recv_buf[256];
    char old_pass[PASS_LEN], new_pass[PASS_LEN], confirm_pass[PASS_LEN];
    int found = 0;
    off_t emp_offset = 0;
    Employee emp;

    pthread_mutex_lock(&employees_mutex);
    int fd_emp = open(EMPLOYEES_DB, O_RDWR);
    if (fd_emp < 0)
    {
        pthread_mutex_unlock(&employees_mutex);
        snprintf(msg, sizeof(msg), "Error opening employee database!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    while (1)
    {
        lock_record(fd_emp, emp_offset, sizeof(Employee), F_RDLCK);
        ssize_t r = read(fd_emp, &emp, sizeof(Employee));
        lock_record(fd_emp, emp_offset, sizeof(Employee), F_UNLCK);

        if (r < (ssize_t)sizeof(Employee))
            break;

        if (strcmp(emp.email, email) == 0)
        {
            found = 1;
            break;
        }
        emp_offset += sizeof(Employee);
    }
    close(fd_emp);
    pthread_mutex_unlock(&employees_mutex);

    if (!found)
    {
        snprintf(msg, sizeof(msg), "Manager record not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 2: Ask old password
    snprintf(msg, sizeof(msg), "Enter old password: ");
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(old_pass, recv_buf, sizeof(old_pass) - 1);

    if (strcmp(old_pass, emp.password) != 0)
    {
        snprintf(msg, sizeof(msg), "Old password incorrect!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 3: Ask for new password
    snprintf(msg, sizeof(msg), "Enter new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(new_pass, recv_buf, sizeof(new_pass) - 1);

    snprintf(msg, sizeof(msg), "Confirm new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[strcspn(recv_buf, "\r\n")] = 0;
    strncpy(confirm_pass, recv_buf, sizeof(confirm_pass) - 1);

    if (strcmp(new_pass, confirm_pass) != 0)
    {
        snprintf(msg, sizeof(msg), "Passwords do not match!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 4: Update in EMPLOYEES_DB in-place
    pthread_mutex_lock(&employees_mutex);
    fd_emp = open(EMPLOYEES_DB, O_RDWR);
    if (fd_emp >= 0)
    {
        lock_record(fd_emp, emp_offset, sizeof(Employee), F_WRLCK);
        strncpy(emp.password, new_pass, sizeof(emp.password) - 1);
        lseek(fd_emp, emp_offset, SEEK_SET);
        write(fd_emp, &emp, sizeof(Employee));
        lock_record(fd_emp, emp_offset, sizeof(Employee), F_UNLCK);
        close(fd_emp);
    }
    pthread_mutex_unlock(&employees_mutex);

    // Step 5: Update in USERS_DB in-place
    pthread_mutex_lock(&users_mutex);
    int fd_user = open(USERS_DB, O_RDWR);
    if (fd_user >= 0)
    {
        User u;
        off_t u_off = 0;
        while (1)
        {
            lock_record(fd_user, u_off, sizeof(User), F_WRLCK);
            ssize_t r = read(fd_user, &u, sizeof(User));
            if (r < (ssize_t)sizeof(User))
            {
                lock_record(fd_user, u_off, sizeof(User), F_UNLCK);
                break;
            }

            if (strcmp(u.email, email) == 0)
            {
                strncpy(u.password, new_pass, sizeof(u.password) - 1);
                lseek(fd_user, u_off, SEEK_SET);
                write(fd_user, &u, sizeof(User));
                lock_record(fd_user, u_off, sizeof(User), F_UNLCK);
                break;
            }

            lock_record(fd_user, u_off, sizeof(User), F_UNLCK);
            u_off += sizeof(User);
        }
        close(fd_user);
    }
    pthread_mutex_unlock(&users_mutex);

    snprintf(msg, sizeof(msg), "Password updated successfully!\n");
    send(client_sock, msg, strlen(msg), 0);
}

// Manager Menu loop
void manager_menu(int client_sock, char *email)
{
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];

    const char *menu =
        "\n--- Manager Session Started ---\n"
        "\nManager Menu:\n"
        "1. Activate/Deactivate Customer Accounts\n"
        "2. List all employees\n"
        "3. List of all unassigned loan applications\n"
        "4. Assign Loan Application to Employee\n"
        "5. Review Customer Feedback\n"
        "6. Change Your Own Password\n"
        "7. Logout\n"
        "Choice: ";

    send(client_sock, menu, strlen(menu), 0);

    while (1)
    {
        memset(recv_buf, 0, sizeof(recv_buf));
        int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
        {
            printf("Manager with email %s disconnected unexpectedly.\n", email);
            remove_active_user(email);
            break;
        }

        recv_buf[strcspn(recv_buf, "\r\n")] = 0;

        if (strcmp(recv_buf, "1") == 0)
            activate_deactivate_customer(client_sock);
        else if (strcmp(recv_buf, "2") == 0)
            list_all_employees(client_sock);
        else if (strcmp(recv_buf, "3") == 0)
            list_unassigned_loans(client_sock);
        else if (strcmp(recv_buf, "4") == 0)
            assign_loan_to_employee(client_sock);
        else if (strcmp(recv_buf, "5") == 0)
            review_customer_feedback(client_sock);
        else if (strcmp(recv_buf, "6") == 0)
            change_manager_own_password(client_sock, email);
        else if (strcmp(recv_buf, "7") == 0)
        {
            remove_active_user(email);
            snprintf(send_buf, sizeof(send_buf), "Manager %s logged out successfully.\n", email);
            send(client_sock, send_buf, strlen(send_buf), 0);
            break;
        }
        else
        {
            send(client_sock, "Invalid option!\n", 16, 0);
        }

        send(client_sock, menu, strlen(menu), 0);
    }
}
