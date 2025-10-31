#include "server.h"

void add_new_customer(int client_sock)
{
    char phone_number[50], email[100], password[100], address[200], account_type[50];
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE], input_buf[BUF_SIZE];

    int account_no = 0, loan_id = 0, feedback_id = 0, transaction_id = 0, employee_id = 0;
    int new_account_no;

    // ==========================
    // 🔒 CRITICAL SECTION START
    // ==========================

    int fd = open(ACCOUNT_FILE, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Unable to open account file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    struct flock lock;
    lock.l_type = F_WRLCK; // Write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    fcntl(fd, F_SETLKW, &lock); // Wait for lock

    FILE *idfile = fdopen(fd, "r+");
    if (idfile == NULL)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: fdopen failed.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        close(fd);
        return;
    }

    if (fscanf(idfile, "%d %d %d %d %d", &account_no, &loan_id, &feedback_id, &transaction_id, &employee_id) != 5)
        account_no = 0;

    new_account_no = account_no + 1;

    rewind(idfile);
    fprintf(idfile, "%d %d %d %d %d\n", new_account_no, loan_id, feedback_id, transaction_id, employee_id);
    fflush(idfile);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    fclose(idfile);

    // ==========================
    // 🔓 CRITICAL SECTION END
    // ==========================

    // Step 1: Phone number
    snprintf(send_buf, sizeof(send_buf), "Enter customer phone number: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    sscanf(recv_buf, "%[^\n]", input_buf);
    strncpy(phone_number, input_buf, sizeof(phone_number));

    // Step 2: Email
    snprintf(send_buf, sizeof(send_buf), "Enter customer email: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    sscanf(recv_buf, "%[^\n]", input_buf);
    strncpy(email, input_buf, sizeof(email));

    // Step 3: Password
    snprintf(send_buf, sizeof(send_buf), "Enter password for customer: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    sscanf(recv_buf, "%[^\n]", input_buf);
    strncpy(password, input_buf, sizeof(password));

    // Step 4: Address
    snprintf(send_buf, sizeof(send_buf), "Enter customer address: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    sscanf(recv_buf, "%[^\n]", input_buf);
    strncpy(address, input_buf, sizeof(address));

    // Step 5: Account type
    snprintf(send_buf, sizeof(send_buf), "Enter account type (savings/current): ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    sscanf(recv_buf, "%[^\n]", input_buf);
    strncpy(account_type, input_buf, sizeof(account_type));

    // Step 6: Save customer record
    FILE *fp = fopen(CUSTOMERS, "a");
    if (fp == NULL)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Unable to open customer file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }
    fprintf(fp, "%d %s %s %s %s %s\n", new_account_no, phone_number, email, password, address, account_type);
    fclose(fp);

    // Step 7: Add login info with activation field = 1
    FILE *uf = fopen(USERS, "a");
    if (uf == NULL)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Unable to open users file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }
    fprintf(uf, "%s %s user 1\n", email, password); // 1 = active
    fclose(uf);

    // Step 8: Initialize balance file
    FILE *bf = fopen("d_balance.txt", "a");
    if (bf == NULL)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Unable to open balance file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }
    fprintf(bf, "%d %d\n", new_account_no, 0);
    fclose(bf);

    // Step 9: Success message
    snprintf(send_buf, sizeof(send_buf),
             "\n✅ Customer added successfully!\nAccount Number: %d\nEmail: %s\nAccount Type: %s\nStatus: ACTIVE\n",
             new_account_no, email, account_type);
    send(client_sock, send_buf, strlen(send_buf), 0);
}

void approve_reject_loans(int client_sock)
{
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    char loan_id[50], decision[50], reason[256];

    memset(send_buf, 0, sizeof(send_buf));
    memset(recv_buf, 0, sizeof(recv_buf));

    // Step 1: Ask for loan ID
    snprintf(send_buf, sizeof(send_buf), "Enter Loan ID to approve/reject: ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf), 0);
    strncpy(loan_id, recv_buf, sizeof(loan_id));
    loan_id[strcspn(loan_id, "\n")] = '\0';

    // Step 2: Ask approve or reject
    snprintf(send_buf, sizeof(send_buf), "Approve or Reject? (type 'approve' or 'reject'): ");
    send(client_sock, send_buf, strlen(send_buf), 0);
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(client_sock, recv_buf, sizeof(recv_buf), 0);
    strncpy(decision, recv_buf, sizeof(decision));
    decision[strcspn(decision, "\n")] = '\0';

    // Step 3: If reject, ask reason
    if (strcasecmp(decision, "reject") == 0)
    {
        snprintf(send_buf, sizeof(send_buf), "Enter reason for rejection: ");
        send(client_sock, send_buf, strlen(send_buf), 0);
        memset(recv_buf, 0, sizeof(recv_buf));
        recv(client_sock, recv_buf, sizeof(recv_buf), 0);
        strncpy(reason, recv_buf, sizeof(reason));
        reason[strcspn(reason, "\n")] = '\0';
    }

    // Step 4: Open loan file
    FILE *fp = fopen("d_loan.txt", "r");
    if (!fp)
    {
        snprintf(send_buf, sizeof(send_buf), "Error opening d_loan.txt file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    FILE *temp = fopen("temp.txt", "w");
    if (!temp)
    {
        fclose(fp);
        snprintf(send_buf, sizeof(send_buf), "Error creating temp file.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    char line[1024];
    int found = 0, updated = 0;
    int cust_id = 0;
    double amount = 0;

    while (fgets(line, sizeof(line), fp))
    {
        char copy[1024];
        strcpy(copy, line);

        // Parse: customer_id loan_id amount type status emp_id ...
        int id, loanid;
        double amt;
        char type[50], status[50], emp[50], other[256];
        int parsed = sscanf(copy, "%d %d %lf %s %s %s %[^\n]", &id, &loanid, &amt, type, status, emp, other);

        if (parsed >= 6 && loanid == atoi(loan_id))
        {
            found = 1;
            cust_id = id;
            amount = amt;

            // ✅ Check if already finalized
            if (strcasecmp(status, "approved") == 0 || strcasecmp(status, "rejected") == 0)
            {
                fputs(line, temp);
                snprintf(send_buf, sizeof(send_buf),
                         "\n[SERVER] Loan ID %s is already %s. No further action taken.\n",
                         loan_id, status);
                send(client_sock, send_buf, strlen(send_buf), 0);
                fclose(fp);
                fclose(temp);
                remove("temp.txt");
                return;
            }

            // ✅ Process pending loan
            char updated_line[1024];
            if (strcasecmp(decision, "approve") == 0)
            {
                snprintf(updated_line, sizeof(updated_line),
                         "%d %d %.2f %s approved %s %s\n",
                         id, loanid, amt, type, emp, other);
                updated = 1;
            }
            else if (strcasecmp(decision, "reject") == 0)
            {
                snprintf(updated_line, sizeof(updated_line),
                         "%d %d %.2f %s rejected-%s %s %s\n",
                         id, loanid, amt, type, reason, emp, other);
                updated = 1;
            }
            else
            {
                strcpy(updated_line, line);
            }

            fputs(updated_line, temp);
        }
        else
        {
            fputs(line, temp);
        }
    }

    fclose(fp);
    fclose(temp);

    remove("d_loan.txt");
    rename("temp.txt", "d_loan.txt");

    // Step 5: Update balance if approved
    if (found && updated && strcasecmp(decision, "approve") == 0)
    {
        FILE *fb = fopen("d_balance.txt", "r");
        FILE *ftemp = fopen("temp_balance.txt", "w");
        if (!fb || !ftemp)
        {
            snprintf(send_buf, sizeof(send_buf), "Error updating d_balance.txt file.\n");
            send(client_sock, send_buf, strlen(send_buf), 0);
            if (fb)
                fclose(fb);
            if (ftemp)
                fclose(ftemp);
            return;
        }

        char line2[256];
        while (fgets(line2, sizeof(line2), fb))
        {
            int cid;
            double balance;
            if (sscanf(line2, "%d %lf", &cid, &balance) == 2)
            {
                if (cid == cust_id)
                {
                    balance += amount; // Add loan amount
                }
                fprintf(ftemp, "%d %.2f\n", cid, balance);
            }
        }

        fclose(fb);
        fclose(ftemp);

        remove("d_balance.txt");
        rename("temp_balance.txt", "d_balance.txt");
    }

    // Step 6: Send final response
    if (!found)
    {
        snprintf(send_buf, sizeof(send_buf),
                 "\n[SERVER] Loan ID %s not found in records.\n", loan_id);
    }
    else if (updated)
    {
        if (strcasecmp(decision, "approve") == 0)
        {
            snprintf(send_buf, sizeof(send_buf),
                     "\n[SERVER] Loan ID %s APPROVED successfully.\nAmount %.2f added to Customer ID %d’s balance.\n",
                     loan_id, amount, cust_id);
        }
        else if (strcasecmp(decision, "reject") == 0)
        {
            snprintf(send_buf, sizeof(send_buf),
                     "\n[SERVER] Loan ID %s REJECTED.\nReason: %s\n", loan_id, reason);
        }
        else
        {
            snprintf(send_buf, sizeof(send_buf),
                     "\n[SERVER] Invalid action.\n");
        }
    }

    send(client_sock, send_buf, strlen(send_buf), 0);
}

void view_assigned_loans(int client_sock, char *email)
{
    char send_buf[BUF_SIZE], recv_buf[BUF_SIZE];
    memset(send_buf, 0, sizeof(send_buf));
    memset(recv_buf, 0, sizeof(recv_buf));

    FILE *emp_fp = fopen(EMPLOYEE, "r");
    if (!emp_fp)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Could not open EMPLOYEE database.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    int emp_id = -1;
    char line[512];

    // Format in EMPLOYEE file:
    // id name email password phone address role department
    while (fgets(line, sizeof(line), emp_fp))
    {
        char id_str[10], name[50], mail[100], pass[50], phone[20], addr[100], role[20], dept[50];
        if (sscanf(line, "%s %s %s %s %s %s %s %s",
                   id_str, name, mail, pass, phone, addr, role, dept) == 8)
        {
            if (strcmp(mail, email) == 0)
            {
                emp_id = atoi(id_str);
                break;
            }
        }
    }
    fclose(emp_fp);

    if (emp_id == -1)
    {
        snprintf(send_buf, sizeof(send_buf),
                 "Error: Employee not found for email: %s\n", email);
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    FILE *loan_fp = fopen(LOAN, "r");
    if (!loan_fp)
    {
        snprintf(send_buf, sizeof(send_buf), "Error: Could not open LOAN database.\n");
        send(client_sock, send_buf, strlen(send_buf), 0);
        return;
    }

    snprintf(send_buf, sizeof(send_buf),
             "\nPending Loans Assigned to You (Employee ID: %d)\n", emp_id);
    send(client_sock, send_buf, strlen(send_buf), 0);

    int found = 0;
    while (fgets(line, sizeof(line), loan_fp))
    {
        int cust_id, loan_id, amount;
        char type[50], status[50], assigned_emp[10], dash[10], timestamp[50];

        // Format:
        // customer_id loan_id amount type status emp_id - timestamp
        if (sscanf(line, "%d %d %d %s %s %s %s %s",
                   &cust_id, &loan_id, &amount, type, status, assigned_emp, dash, timestamp) >= 7)
        {
            int current_emp_id = atoi(assigned_emp);

            // ✅ Filter: Only show pending loans assigned to this employee
            if (current_emp_id == emp_id && strcmp(status, "pending") == 0)
            {
                found = 1;
                snprintf(send_buf, sizeof(send_buf),
                         "Loan ID: %d | Customer ID: %d | Amount: %d | Type: %s | Status: %s | Date: %s\n",
                         loan_id, cust_id, amount, type, status, timestamp);
                send(client_sock, send_buf, strlen(send_buf), 0);
                memset(send_buf, 0, sizeof(send_buf));
            }
        }
    }

    if (!found)
    {
        snprintf(send_buf, sizeof(send_buf),
                 "No pending loans assigned to your ID (%d).\n", emp_id);
        send(client_sock, send_buf, strlen(send_buf), 0);
    }

    fclose(loan_fp);
}

void modify_customer_details_emp(int client_sock, char *email)
{
    char buffer[BUF_SIZE], recv_buf[BUF_SIZE];
    int acc_no, found = 0;

    // Step 1: Ask for account number
    send(client_sock, "Enter customer account number: ", 32, 0);
    int n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
    if (n <= 0)
        return;
    recv_buf[n] = '\0';
    acc_no = atoi(recv_buf);

    // Step 2: Search for customer
    FILE *fp = fopen(CUSTOMERS, "r");
    if (!fp)
    {
        send(client_sock, "Error opening customer file!\n", 29, 0);
        return;
    }

    char line[512], cust_phone[50], cust_email[100], cust_pass[50], cust_address[200], cust_type[50];
    while (fgets(line, sizeof(line), fp))
    {
        int file_acc;
        char temp[512];
        sscanf(line, "%d %49s %99s %49s %199[^\n]", &file_acc, cust_phone, cust_email, cust_pass, temp);
        if (file_acc == acc_no)
        {
            found = 1;
            // Extract type (last word)
            char *last_space = strrchr(temp, ' ');
            if (last_space)
            {
                strcpy(cust_type, last_space + 1);
                *last_space = '\0';
            }
            else
                cust_type[0] = '\0';
            strcpy(cust_address, temp);
            break;
        }
    }
    fclose(fp);

    if (!found)
    {
        send(client_sock, "Account not found!\n", 19, 0);
        return;
    }

    // Step 3: Show modify menu
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

        n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0)
            return;
        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\n")] = 0;

        int choice = atoi(recv_buf);

        if (choice == 1)
        {
            snprintf(buffer, BUF_SIZE,
                     "Account: %d\nPhone: %s\nEmail: %s\nPassword: %s\nAddress: %s\nType: %s\n",
                     acc_no, cust_phone, cust_email, cust_pass, cust_address, cust_type);
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice >= 2 && choice <= 6)
        {
            const char *prompt;
            if (choice == 2)
                prompt = "Enter new email: ";
            else if (choice == 3)
                prompt = "Enter new password: ";
            else if (choice == 4)
                prompt = "Enter new phone: ";
            else if (choice == 5)
                prompt = "Enter new address: ";
            else
                prompt = "Enter new account type: ";

            send(client_sock, prompt, strlen(prompt), 0);

            n = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n <= 0)
                return;
            recv_buf[n] = '\0';
            recv_buf[strcspn(recv_buf, "\n")] = 0;

            char old_email[100];
            strcpy(old_email, cust_email); // store old email before change

            // Update local variables
            if (choice == 2)
                strcpy(cust_email, recv_buf);
            else if (choice == 3)
                strcpy(cust_pass, recv_buf);
            else if (choice == 4)
                strcpy(cust_phone, recv_buf);
            else if (choice == 5)
                strcpy(cust_address, recv_buf);
            else if (choice == 6)
                strcpy(cust_type, recv_buf);

            // Update d_customer.txt
            FILE *fpr = fopen(CUSTOMERS, "r");
            FILE *fpw = fopen("temp_customer.txt", "w");
            if (!fpr || !fpw)
            {
                send(client_sock, "File operation error!\n", 23, 0);
                if (fpr)
                    fclose(fpr);
                if (fpw)
                    fclose(fpw);
                return;
            }

            while (fgets(line, sizeof(line), fpr))
            {
                int file_acc;
                char phone[50], email[100], pass[50], address[200], type[50];
                char temp[512];
                sscanf(line, "%d %49s %99s %49s %199[^\n]", &file_acc, phone, email, pass, temp);

                char *last_space = strrchr(temp, ' ');
                if (last_space)
                {
                    strcpy(type, last_space + 1);
                    *last_space = '\0';
                }
                else
                    type[0] = '\0';
                strcpy(address, temp);

                if (file_acc == acc_no)
                {
                    if (choice == 2)
                        strcpy(email, cust_email);
                    else if (choice == 3)
                        strcpy(pass, cust_pass);
                    else if (choice == 4)
                        strcpy(phone, cust_phone);
                    else if (choice == 5)
                        strcpy(address, cust_address);
                    else if (choice == 6)
                        strcpy(type, cust_type);
                }

                fprintf(fpw, "%d %s %s %s %s %s\n", file_acc, phone, email, pass, address, type);
            }
            fclose(fpr);
            fclose(fpw);
            remove(CUSTOMERS);
            rename("temp_customer.txt", CUSTOMERS);

            // --- Update d_login.txt (USERS) ---
            if (choice == 2 || choice == 3)
            {
                FILE *flog = fopen(USERS, "r");
                FILE *ftmp = fopen("temp_login.txt", "w");
                if (!flog || !ftmp)
                {
                    send(client_sock, "Login file update error!\n", 26, 0);
                    if (flog)
                        fclose(flog);
                    if (ftmp)
                        fclose(ftmp);
                    return;
                }

                char login_line[256];
                while (fgets(login_line, sizeof(login_line), flog))
                {
                    char login_email[100], login_pass[100], login_role[50];
                    int activation = 1; // Default if missing

                    // Expecting: EMAIL PASSWORD ROLE ACTIVATION
                    if (sscanf(login_line, "%99s %99s %49s %d", login_email, login_pass, login_role, &activation) < 4)
                        continue;

                    // --- If email is changed ---
                    if (choice == 2)
                    {
                        if (strcmp(login_email, old_email) == 0)
                            fprintf(ftmp, "%s %s %s %d\n", cust_email, login_pass, login_role, activation);
                        else
                            fputs(login_line, ftmp);
                    }
                    // --- If password is changed ---
                    else if (choice == 3)
                    {
                        if (strcmp(login_email, cust_email) == 0)
                            fprintf(ftmp, "%s %s %s %d\n", login_email, cust_pass, login_role, activation);
                        else
                            fputs(login_line, ftmp);
                    }
                }

                fclose(flog);
                fclose(ftmp);

                remove(USERS);
                rename("temp_login.txt", USERS);
            }

            snprintf(buffer, BUF_SIZE, "Update successful!\n");
            send(client_sock, buffer, strlen(buffer), 0);
        }
        else if (choice == 7)
        {
            employee_menu(client_sock, email);
            return;
        }
        else
        {
            send(client_sock, "Invalid choice!\n", 16, 0);
        }
    }
}

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

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "1") == 0)
        {
            add_new_customer(client_sock);
        }
        else if (strcmp(buffer, "2") == 0)
        {
            modify_customer_details_emp(client_sock, email);
        }
        else if (strcmp(buffer, "3") == 0)
        {
            view_assigned_loans(client_sock, email);
        }
        else if (strcmp(buffer, "4") == 0)
        {
            approve_reject_loans(client_sock);
        }
        else if (strcmp(buffer, "5") == 0) // Logout
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
