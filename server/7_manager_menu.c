#include "server.h"

//================================ MANAGER MENU =============================================================>

void activate_deactivate_customer(int client_sock)
{
    char msg[256], recv_buf[256];
    char email[100];
    int status_found = 0;

    // Step 1: Ask for customer email
    sprintf(msg, "Enter customer email:"); // ✅ match client
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(email, recv_buf);

    // Step 2: Ask for action (1 = Activate, 0 = Deactivate)
    sprintf(msg, "Enter 1 to activate or 0 to deactivate:");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    int activation = atoi(recv_buf);
    if (activation != 0 && activation != 1)
    {
        sprintf(msg, "Invalid choice! Please enter 1 or 0 only.\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 3: Open login file
    FILE *fp = fopen(USERS, "r");
    FILE *temp = fopen("temp_users.txt", "w");
    if (!fp || !temp)
    {
        sprintf(msg, "Error opening login file!\n");
        send(client_sock, msg, strlen(msg), 0);
        if (fp)
            fclose(fp);
        if (temp)
            fclose(temp);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        char file_email[100], password[50], role[20], act_status[10];
        int fields = sscanf(line, "%99s %49s %19s %9s", file_email, password, role, act_status);

        if (fields < 3)
            continue;

        if (strcmp(file_email, email) == 0)
        {
            status_found = 1;
            if (fields == 4)
                fprintf(temp, "%s %s %s %d\n", file_email, password, role, activation);
            else
                fprintf(temp, "%s %s %s %d\n", file_email, password, role, activation);
        }
        else
        {
            // Keep other users as is
            fputs(line, temp);
        }
    }

    fclose(fp);
    fclose(temp);
    remove(USERS);
    rename("temp_users.txt", USERS);

    // Step 4: Send result
    if (status_found)
    {
        if (activation == 1)
            sprintf(msg, "Customer account '%s' ACTIVATED successfully.\n", email);
        else
            sprintf(msg, "Customer account '%s' DEACTIVATED successfully.\n", email);
    }
    else
        sprintf(msg, "Customer email '%s' not found in records!\n", email);

    send(client_sock, msg, strlen(msg), 0);

    // Step 5: Show manager menu again
    usleep(100000);
    const char *menu =
        "\nManager Menu:\n"
        "1. Activate/Deactivate Customer Accounts\n"
        "2. List all employees\n"
        "3. List of all unassigned loan applications\n"
        "4. Assign Loan Application to Employee\n"
        "5. Review Customer Feedback\n"
        "6. Change Your Own Password\n"
        "7. Logout\n"
        "8. Exit\n"
        "Choice: ";
    send(client_sock, menu, strlen(menu), 0);
}

void list_all_employees(int client_sock)
{
    FILE *fp = fopen(EMPLOYEE, "r");
    if (!fp)
    {
        send(client_sock, "Error opening employee file!\n", 29, 0);
        return;
    }

    char line[BUF_SIZE];

    // Table header (without password)
    char header[BUF_SIZE];
    snprintf(header, BUF_SIZE, "%-5s %-15s %-25s %-15s %-15s %-15s %-10s\n",
             "ID", "NAME", "EMAIL", "PHONE", "ADDRESS", "POSITION", "DEPT");
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "------------------------------------------------------------------------------------------------------\n", 102, 0);

    // Read file line by line
    while (fgets(line, sizeof(line), fp))
    {
        int id;
        char name[50], email[100], pass[50], phone[50], address[50], pos[50], dept[50];

        // Read file fields
        sscanf(line, "%d %49s %99s %49s %49s %49s %49s %49s",
               &id, name, email, pass, phone, address, pos, dept);

        char buffer[BUF_SIZE];
        snprintf(buffer, BUF_SIZE, "\n%-5d %-15s %-25s %-15s %-15s %-15s %-10s",
                 id, name, email, phone, address, pos, dept);
        send(client_sock, buffer, strlen(buffer), 0);
    }

    fclose(fp);
}

void list_unassigned_loans(int client_sock)
{
    FILE *fp = fopen(LOAN, "r");
    if (!fp)
    {
        send(client_sock, "Error opening loan file!\n", 26, 0);
        return;
    }

    // Prepare table header
    char header[BUF_SIZE];
    snprintf(header, BUF_SIZE, "%-15s %-10s %-12s %-10s %-10s %-15s %-20s\n",
             "ACCOUNT_NUMBER", "LOAN_ID", "AMOUNT", "TYPE", "STATUS", "ASSIGNED_TO", "TIMESTAMP");
    send(client_sock, header, strlen(header), 0);
    send(client_sock, "---------------------------------------------------------------------------------------------\n", 93, 0);

    // Read all lines into memory (for sorting)
    char lines[100][BUF_SIZE]; // adjust size if needed
    int count = 0;

    while (fgets(lines[count], sizeof(lines[count]), fp))
    {
        count++;
    }
    fclose(fp);

    // Simple bubble sort by timestamp (oldest to newest)
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = 0; j < count - i - 1; j++)
        {
            char timestamp1[30], timestamp2[30];
            int acc, loanid, assigned;

            sscanf(lines[j], "%d %d %*s %*s %*s %d %*s %s", &acc, &loanid, &assigned, timestamp1);
            sscanf(lines[j + 1], "%d %d %*s %*s %*s %d %*s %s", &acc, &loanid, &assigned, timestamp2);

            // Compare timestamps lexicographically (works since format is YYYY-MM-DD HH:MM:SS)
            if (strcmp(timestamp1, timestamp2) > 0)
            {
                char tmp[BUF_SIZE];
                strcpy(tmp, lines[j]);
                strcpy(lines[j], lines[j + 1]);
                strcpy(lines[j + 1], tmp);
            }
        }
    }

    // Print only unassigned loans
    for (int i = 0; i < count; i++)
    {
        int acc_no, loan_id, assigned_to;
        char amount[50], type[50], status[50], reason[100], timestamp[30];

        sscanf(lines[i], "%d %d %49s %49s %49s %d %99s %29[^\n]",
               &acc_no, &loan_id, amount, type, status, &assigned_to, reason, timestamp);

        if (assigned_to == 0) // unassigned loans only
        {
            char buffer[BUF_SIZE];
            snprintf(buffer, BUF_SIZE, "\n%-15d %-10d %-12s %-10s %-10s %-15d %-20s",
                     acc_no, loan_id, amount, type, status, assigned_to, timestamp);
            send(client_sock, buffer, strlen(buffer), 0);
        }
    }
}

void assign_loan_to_employee(int client_sock)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int loan_id, emp_id, found = 0;

    // Step 1: Ask for Loan ID
    send(client_sock, "Enter Loan ID to assign: ", 25, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    loan_id = atoi(recv_buf);

    // Step 2: Ask for Employee ID
    send(client_sock, "Enter Employee ID to assign to: ", 32, 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    emp_id = atoi(recv_buf);

    // Step 3: Open loan file and update assigned_to
    FILE *fpr = fopen(LOAN, "r");
    FILE *fpw = fopen("temp_loan.txt", "w");
    if (!fpr || !fpw)
    {
        send(client_sock, "Error opening loan file!\n", 26, 0);
        if (fpr)
            fclose(fpr);
        if (fpw)
            fclose(fpw);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fpr))
    {
        int acc_no, l_id, assigned_to;
        char amount[50], type[50], status[50], reason[100], timestamp[30];

        sscanf(line, "%d %d %49s %49s %49s %d %99s %29[^\n]",
               &acc_no, &l_id, amount, type, status, &assigned_to, reason, timestamp);

        if (l_id == loan_id)
        {
            assigned_to = emp_id; // assign employee
            found = 1;
        }

        fprintf(fpw, "%d %d %s %s %s %d %s %s\n",
                acc_no, l_id, amount, type, status, assigned_to, reason, timestamp);
    }

    fclose(fpr);
    fclose(fpw);
    remove(LOAN);
    rename("temp_loan.txt", LOAN);

    if (found)
        send(client_sock, "Loan assigned successfully!\n", 28, 0);
    else
        send(client_sock, "Loan ID not found!\n", 20, 0);
}

void review_customer_feedback(int client_sock)
{
    FILE *fp = fopen(FEEDBACK, "r");
    if (!fp)
    {
        send(client_sock, "Error opening feedback file!\n", 29, 0);
        return;
    }

    char line[BUF_SIZE];
    char **feedback_lines = NULL;
    int count = 0;

    // Step 1: Read all lines into an array
    while (fgets(line, sizeof(line), fp))
    {
        feedback_lines = realloc(feedback_lines, sizeof(char *) * (count + 1));
        feedback_lines[count] = strdup(line);
        count++;
    }
    fclose(fp);

    if (count == 0)
    {
        send(client_sock, "No feedback available!\n", 23, 0);
        return;
    }

    // Step 2: Sort feedback by timestamp (newest first)
    // Assuming timestamp format: YYYY-MM-DD HH:MM:SS at the end of the line
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            char ts_i[20], ts_j[20];
            sscanf(strrchr(feedback_lines[i], ' ') - 8, "%19[^\n]", ts_i); // extract last 19 chars as timestamp
            sscanf(strrchr(feedback_lines[j], ' ') - 8, "%19[^\n]", ts_j);

            if (strcmp(ts_i, ts_j) < 0) // newer first
            {
                char *tmp = feedback_lines[i];
                feedback_lines[i] = feedback_lines[j];
                feedback_lines[j] = tmp;
            }
        }
    }

    // Step 3: Send all feedback to client
    for (int i = 0; i < count; i++)
    {
        send(client_sock, feedback_lines[i], strlen(feedback_lines[i]), 0);
        free(feedback_lines[i]);
    }
    free(feedback_lines);
}

void change_manager_own_password(int client_sock, const char *email)
{
    char msg[256], recv_buf[256];
    char old_pass[50], new_pass[50], confirm_pass[50];
    int found = 0;
    char current_pass[50];
    char line[512];

    // Step 1: Find manager in d_employee.txt and get current password
    FILE *fp = fopen(EMPLOYEE, "r");
    if (!fp)
    {
        sprintf(msg, "Error opening employee file!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    while (fgets(line, sizeof(line), fp))
    {
        int file_id;
        char name[50], emp_email[100], pass[50], phone[50];
        char address[200], position[50], dept[50];

        if (sscanf(line, "%d %49s %99s %49s %49s %199s %49s %49s",
                   &file_id, name, emp_email, pass, phone, address, position, dept) != 8)
            continue;

        if (strcmp(emp_email, email) == 0)
        {
            strcpy(current_pass, pass);
            found = 1;
            break;
        }
    }
    fclose(fp);

    if (!found)
    {
        sprintf(msg, "Manager record not found!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 2: Ask old password
    sprintf(msg, "Enter old password: ");
    send(client_sock, msg, strlen(msg), 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(old_pass, recv_buf);

    if (strcmp(old_pass, current_pass) != 0)
    {
        sprintf(msg, "Old password incorrect!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 3: Ask for new password
    sprintf(msg, "Enter new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(new_pass, recv_buf);

    sprintf(msg, "Confirm new password: ");
    send(client_sock, msg, strlen(msg), 0);
    n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    recv_buf[strcspn(recv_buf, "\n")] = 0;
    strcpy(confirm_pass, recv_buf);

    if (strcmp(new_pass, confirm_pass) != 0)
    {
        sprintf(msg, "Passwords do not match!\n");
        send(client_sock, msg, strlen(msg), 0);
        return;
    }

    // Step 4: Update in d_employee.txt
    FILE *fe = fopen(EMPLOYEE, "r");
    FILE *fe_temp = fopen("temp_employee.txt", "w");
    if (!fe || !fe_temp)
    {
        sprintf(msg, "Error opening employee files!\n");
        send(client_sock, msg, strlen(msg), 0);
        if (fe)
            fclose(fe);
        if (fe_temp)
            fclose(fe_temp);
        return;
    }

    while (fgets(line, sizeof(line), fe))
    {
        int file_id;
        char name[50], emp_email[100], pass[50], phone[50];
        char address[200], position[50], dept[50];

        if (sscanf(line, "%d %49s %99s %49s %49s %199s %49s %49s",
                   &file_id, name, emp_email, pass, phone, address, position, dept) != 8)
            continue;

        if (strcmp(emp_email, email) == 0)
            fprintf(fe_temp, "%d %s %s %s %s %s %s %s\n",
                    file_id, name, emp_email, new_pass, phone, address, position, dept);
        else
            fputs(line, fe_temp);
    }

    fclose(fe);
    fclose(fe_temp);
    remove(EMPLOYEE);
    rename("temp_employee.txt", EMPLOYEE);

    // Step 5: Update in d_login.txt (with ACTIVATION field kept as is)
    FILE *fl = fopen(USERS, "r");
    FILE *fl_temp = fopen("temp_login.txt", "w");
    if (!fl || !fl_temp)
    {
        sprintf(msg, "Error opening login files!\n");
        send(client_sock, msg, strlen(msg), 0);
        if (fl)
            fclose(fl);
        if (fl_temp)
            fclose(fl_temp);
        return;
    }

    while (fgets(line, sizeof(line), fl))
    {
        char login_email[100], pass[50], role[20], activation[20];

        // File format: email password role ACTIVATION
        int fields = sscanf(line, "%99s %49s %19s %19s",
                            login_email, pass, role, activation);

        if (fields < 3)
            continue;

        if (strcmp(login_email, email) == 0)
        {
            if (fields == 4)
                fprintf(fl_temp, "%s %s %s %s\n", login_email, new_pass, role, activation);
            else
                fprintf(fl_temp, "%s %s %s\n", login_email, new_pass, role);
        }
        else
            fputs(line, fl_temp);
    }

    fclose(fl);
    fclose(fl_temp);
    remove(USERS);
    rename("temp_login.txt", USERS);

    // Step 6: Success message and show menu again
    sprintf(msg, "Password updated successfully!\n");
    send(client_sock, msg, strlen(msg), 0);

    usleep(100000); // small delay to flush

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
        "8. Exit\n"
        "Choice: ";
    send(client_sock, menu, strlen(menu), 0);
}

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

        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\n")] = 0;

        if (strcmp(recv_buf, "1") == 0)
        {
            activate_deactivate_customer(client_sock);
        }
        else if (strcmp(recv_buf, "2") == 0)
        {
            list_all_employees(client_sock);
        }
        else if (strcmp(recv_buf, "3") == 0)
        {
            list_unassigned_loans(client_sock);
        }
        else if (strcmp(recv_buf, "4") == 0)
        {
            assign_loan_to_employee(client_sock);
        }
        else if (strcmp(recv_buf, "5") == 0)
        {
            review_customer_feedback(client_sock);
        }
        else if (strcmp(recv_buf, "6") == 0)
        {
            change_manager_own_password(client_sock, email);
        }
        else if (strcmp(recv_buf, "7") == 0)
        {
            remove_active_user(email);
            snprintf(send_buf, sizeof(send_buf),
                     "Manager %s logged out successfully.\n", email);
            send(client_sock, send_buf, strlen(send_buf), 0);
            break;
        }
        else
        {
            strcpy(send_buf, "Invalid option!\n");
            send(client_sock, send_buf, strlen(send_buf), 0);
        }

        send(client_sock, menu, strlen(menu), 0);
    }
}
