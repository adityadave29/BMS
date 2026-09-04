#include "server.h"

pthread_mutex_t counters_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t customers_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t employees_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t balance_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t loans_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t transactions_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t feedbacks_mutex = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// Record-Level fcntl Byte-Range Locking
// ============================================================================
int lock_record(int fd, off_t offset, size_t size, short lock_type)
{
    struct flock fl;
    fl.l_type = lock_type;     // F_RDLCK, F_WRLCK, or F_UNLCK
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = size;
    fl.l_pid = getpid();

    int cmd = (lock_type == F_UNLCK) ? F_SETLK : F_SETLKW;
    if (fcntl(fd, cmd, &fl) == -1)
    {
        perror("fcntl lock_record error");
        return -1;
    }
    return 0;
}

// ============================================================================
// Atomic Counter ID Generation
// ============================================================================
static int get_and_increment_counter(int counter_index)
{
    pthread_mutex_lock(&counters_mutex);
    int fd = open(COUNTERS_DB, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("Error opening counters file");
        pthread_mutex_unlock(&counters_mutex);
        return 1;
    }

    // Lock entire counters record
    lock_record(fd, 0, sizeof(Counters), F_WRLCK);

    Counters c;
    memset(&c, 0, sizeof(Counters));
    ssize_t r = read(fd, &c, sizeof(Counters));
    if (r < (ssize_t)sizeof(Counters))
    {
        c.account_no = 0;
        c.loan_id = 0;
        c.feedback_id = 0;
        c.transaction_id = 0;
        c.employee_id = 0;
    }

    int result_id = 0;
    switch (counter_index)
    {
    case 0: c.account_no++; result_id = c.account_no; break;
    case 1: c.loan_id++; result_id = c.loan_id; break;
    case 2: c.feedback_id++; result_id = c.feedback_id; break;
    case 3: c.transaction_id++; result_id = c.transaction_id; break;
    case 4: c.employee_id++; result_id = c.employee_id; break;
    default: result_id = 1; break;
    }

    lseek(fd, 0, SEEK_SET);
    write(fd, &c, sizeof(Counters));

    // Unlock
    lock_record(fd, 0, sizeof(Counters), F_UNLCK);
    close(fd);
    pthread_mutex_unlock(&counters_mutex);

    return result_id;
}

int get_next_account_number(void) { return get_and_increment_counter(0); }
int get_next_loan_id(void) { return get_and_increment_counter(1); }
int get_next_feedback_id(void) { return get_and_increment_counter(2); }
int get_next_transaction_id(void) { return get_and_increment_counter(3); }
int get_next_employee_id(void) { return get_and_increment_counter(4); }

// ============================================================================
// User & Session Helpers
// ============================================================================
int set_user_login_status(const char *email, int is_logged_in)
{
    pthread_mutex_lock(&users_mutex);
    int fd = open(USERS_DB, O_RDWR);
    if (fd < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        return -1;
    }

    User u;
    off_t offset = 0;
    int updated = 0;

    while (1)
    {
        // Acquire write lock before reading/modifying record
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
            u.is_logged_in = is_logged_in;
            lseek(fd, offset, SEEK_SET);
            write(fd, &u, sizeof(User));
            updated = 1;

            lock_record(fd, offset, sizeof(User), F_UNLCK);
            break;
        }

        lock_record(fd, offset, sizeof(User), F_UNLCK);
        offset += sizeof(User);
    }

    close(fd);
    pthread_mutex_unlock(&users_mutex);
    return updated ? 0 : -1;
}

void remove_active_user(const char *email)
{
    set_user_login_status(email, 0);
}

int get_account_number_by_email(const char *email)
{
    pthread_mutex_lock(&customers_mutex);
    int fd = open(CUSTOMERS_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&customers_mutex);
        return -1;
    }

    Customer c;
    off_t offset = 0;
    int acc_no = -1;

    while (1)
    {
        lock_record(fd, offset, sizeof(Customer), F_RDLCK);
        ssize_t r = read(fd, &c, sizeof(Customer));
        lock_record(fd, offset, sizeof(Customer), F_UNLCK);

        if (r < (ssize_t)sizeof(Customer))
            break;

        if (strcmp(c.email, email) == 0)
        {
            acc_no = c.account_no;
            break;
        }
        offset += sizeof(Customer);
    }

    close(fd);
    pthread_mutex_unlock(&customers_mutex);
    return acc_no;
}

double get_balance(int acc_no)
{
    pthread_mutex_lock(&balance_mutex);
    int fd = open(BALANCE_DB, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&balance_mutex);
        return -1.0;
    }

    AccountBalance ab;
    off_t offset = 0;
    double balance = -1.0;

    while (1)
    {
        lock_record(fd, offset, sizeof(AccountBalance), F_RDLCK);
        ssize_t r = read(fd, &ab, sizeof(AccountBalance));
        lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);

        if (r < (ssize_t)sizeof(AccountBalance))
            break;

        if (ab.account_no == acc_no)
        {
            balance = ab.balance;
            break;
        }
        offset += sizeof(AccountBalance);
    }

    close(fd);
    pthread_mutex_unlock(&balance_mutex);
    return balance;
}

void update_balance(int acc_no, double new_balance)
{
    pthread_mutex_lock(&balance_mutex);
    int fd = open(BALANCE_DB, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        pthread_mutex_unlock(&balance_mutex);
        return;
    }

    AccountBalance ab;
    off_t offset = 0;
    int found = 0;

    while (1)
    {
        lock_record(fd, offset, sizeof(AccountBalance), F_WRLCK);
        ssize_t r = read(fd, &ab, sizeof(AccountBalance));

        if (r < (ssize_t)sizeof(AccountBalance))
        {
            // If end of file reached and not found, append
            if (!found)
            {
                ab.account_no = acc_no;
                ab.balance = new_balance;
                lseek(fd, offset, SEEK_SET);
                write(fd, &ab, sizeof(AccountBalance));
                lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            }
            else
            {
                lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            }
            break;
        }

        if (ab.account_no == acc_no)
        {
            ab.balance = new_balance;
            lseek(fd, offset, SEEK_SET);
            write(fd, &ab, sizeof(AccountBalance));
            lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
            found = 1;
            break;
        }

        lock_record(fd, offset, sizeof(AccountBalance), F_UNLCK);
        offset += sizeof(AccountBalance);
    }

    close(fd);
    pthread_mutex_unlock(&balance_mutex);
}

// ============================================================================
// Database Migration & Startup Initialization
// ============================================================================
static void migrate_text_data_if_needed(void)
{
    // 1. Migrate Users if users.dat doesn't exist
    if (access(USERS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_login.txt", "r");
        int fd = open(USERS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char email[100], pass[100], role[50];
            int active = 1, id = 1;
            while (fscanf(txt, "%99s %99s %49s %d", email, pass, role, &active) >= 3)
            {
                User u;
                memset(&u, 0, sizeof(User));
                u.id = id++;
                strncpy(u.email, email, sizeof(u.email) - 1);
                strncpy(u.password, pass, sizeof(u.password) - 1);
                strncpy(u.role, role, sizeof(u.role) - 1);
                u.is_active = active;
                u.is_logged_in = 0;
                write(fd, &u, sizeof(User));
            }
            fclose(txt);
            close(fd);
        }
    }

    // 2. Migrate Customers if customers.dat doesn't exist
    if (access(CUSTOMERS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_customers.txt", "r");
        int fd = open(CUSTOMERS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char line[512];
            while (fgets(line, sizeof(line), txt))
            {
                int acc = 0;
                char phone[50], email[100], pass[50], address[200], type[50];
                if (sscanf(line, "%d %49s %99s %49s %199s %49s", &acc, phone, email, pass, address, type) == 6)
                {
                    Customer c;
                    memset(&c, 0, sizeof(Customer));
                    c.account_no = acc;
                    strncpy(c.phone, phone, sizeof(c.phone) - 1);
                    strncpy(c.email, email, sizeof(c.email) - 1);
                    strncpy(c.password, pass, sizeof(c.password) - 1);
                    strncpy(c.address, address, sizeof(c.address) - 1);
                    strncpy(c.account_type, type, sizeof(c.account_type) - 1);
                    write(fd, &c, sizeof(Customer));
                }
            }
            fclose(txt);
            close(fd);
        }
    }

    // 3. Migrate Employees if employees.dat doesn't exist
    if (access(EMPLOYEES_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_employee.txt", "r");
        int fd = open(EMPLOYEES_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char line[512];
            while (fgets(line, sizeof(line), txt))
            {
                int id = 0;
                char name[50], email[100], pass[50], phone[50], address[200], pos[50], dept[50];
                if (sscanf(line, "%d %49s %99s %49s %49s %199s %49s %49s",
                           &id, name, email, pass, phone, address, pos, dept) == 8)
                {
                    Employee e;
                    memset(&e, 0, sizeof(Employee));
                    e.emp_id = id;
                    strncpy(e.name, name, sizeof(e.name) - 1);
                    strncpy(e.email, email, sizeof(e.email) - 1);
                    strncpy(e.password, pass, sizeof(e.password) - 1);
                    strncpy(e.phone, phone, sizeof(e.phone) - 1);
                    strncpy(e.address, address, sizeof(e.address) - 1);
                    strncpy(e.position, pos, sizeof(e.position) - 1);
                    strncpy(e.department, dept, sizeof(e.department) - 1);
                    write(fd, &e, sizeof(Employee));
                }
            }
            fclose(txt);
            close(fd);
        }
    }

    // 4. Migrate Balance if balance.dat doesn't exist
    if (access(BALANCE_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_balance.txt", "r");
        int fd = open(BALANCE_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            int acc = 0;
            double bal = 0.0;
            while (fscanf(txt, "%d %lf", &acc, &bal) == 2)
            {
                AccountBalance b;
                b.account_no = acc;
                b.balance = bal;
                write(fd, &b, sizeof(AccountBalance));
            }
            fclose(txt);
            close(fd);
        }
    }

    // 5. Migrate Loans if loans.dat doesn't exist
    if (access(LOANS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_loan.txt", "r");
        int fd = open(LOANS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char line[512];
            while (fgets(line, sizeof(line), txt))
            {
                int acc = 0, lid = 0, assigned = 0;
                double amt = 0.0;
                char type[50], status[50], reason[100], time_str[50];
                if (sscanf(line, "%d %d %lf %49s %49s %d %99s %49[^\n]",
                           &acc, &lid, &amt, type, status, &assigned, reason, time_str) >= 5)
                {
                    Loan l;
                    memset(&l, 0, sizeof(Loan));
                    l.account_no = acc;
                    l.loan_id = lid;
                    l.amount = amt;
                    strncpy(l.loan_type, type, sizeof(l.loan_type) - 1);
                    strncpy(l.status, status, sizeof(l.status) - 1);
                    l.assigned_to = assigned;
                    strncpy(l.reason, reason, sizeof(l.reason) - 1);
                    strncpy(l.timestamp, time_str, sizeof(l.timestamp) - 1);
                    write(fd, &l, sizeof(Loan));
                }
            }
            fclose(txt);
            close(fd);
        }
    }

    // 6. Migrate Transactions if transactions.dat doesn't exist
    if (access(TRANSACTIONS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_transaction.txt", "r");
        int fd = open(TRANSACTIONS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char line[512];
            while (fgets(line, sizeof(line), txt))
            {
                int tid = 0, from = 0, to = 0;
                double amt = 0.0, before = 0.0, after = 0.0;
                char time_str[50];
                if (sscanf(line, "%d %d %d %lf %lf %lf %49[^\n]",
                           &tid, &from, &to, &amt, &before, &after, time_str) == 7)
                {
                    Transaction t;
                    memset(&t, 0, sizeof(Transaction));
                    t.transaction_id = tid;
                    t.from_acc = from;
                    t.to_acc = to;
                    t.amount = amt;
                    t.before_bal = before;
                    t.after_bal = after;
                    strncpy(t.timestamp, time_str, sizeof(t.timestamp) - 1);
                    write(fd, &t, sizeof(Transaction));
                }
            }
            fclose(txt);
            close(fd);
        }
    }

    // 7. Migrate Feedbacks if feedbacks.dat doesn't exist
    if (access(FEEDBACKS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_feedback.txt", "r");
        int fd = open(FEEDBACKS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (txt && fd >= 0)
        {
            char line[512];
            while (fgets(line, sizeof(line), txt))
            {
                int fid = 0, acc = 0;
                char ftext[256], time_str[50];
                if (sscanf(line, "%d %d %255s %49[^\n]", &fid, &acc, ftext, time_str) == 4)
                {
                    Feedback fb;
                    memset(&fb, 0, sizeof(Feedback));
                    fb.feedback_id = fid;
                    fb.account_no = acc;
                    strncpy(fb.feedback, ftext, sizeof(fb.feedback) - 1);
                    strncpy(fb.timestamp, time_str, sizeof(fb.timestamp) - 1);
                    write(fd, &fb, sizeof(Feedback));
                }
            }
            fclose(txt);
            close(fd);
        }
    }

    // 8. Migrate Counters if counters.dat doesn't exist
    if (access(COUNTERS_DB, F_OK) != 0)
    {
        FILE *txt = fopen(DATA_DIR "d_account_number.txt", "r");
        int fd = open(COUNTERS_DB, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0)
        {
            Counters c;
            memset(&c, 0, sizeof(Counters));
            if (txt && fscanf(txt, "%d %d %d %d %d",
                              &c.account_no, &c.loan_id, &c.feedback_id, &c.transaction_id, &c.employee_id) == 5)
            {
                fclose(txt);
            }
            else
            {
                c.account_no = 20;
                c.loan_id = 20;
                c.feedback_id = 20;
                c.transaction_id = 60;
                c.employee_id = 30;
            }
            write(fd, &c, sizeof(Counters));
            close(fd);
        }
    }
}

void init_database(void)
{
    // 1. Run migration if starting for the first time
    migrate_text_data_if_needed();

    // 2. Crash recovery: Reset is_logged_in = 0 for ALL users across the database
    pthread_mutex_lock(&users_mutex);
    int fd = open(USERS_DB, O_RDWR);
    if (fd >= 0)
    {
        User u;
        off_t offset = 0;
        while (1)
        {
            lock_record(fd, offset, sizeof(User), F_WRLCK);
            ssize_t r = read(fd, &u, sizeof(User));
            if (r < (ssize_t)sizeof(User))
            {
                lock_record(fd, offset, sizeof(User), F_UNLCK);
                break;
            }

            if (u.is_logged_in != 0)
            {
                u.is_logged_in = 0;
                lseek(fd, offset, SEEK_SET);
                write(fd, &u, sizeof(User));
            }

            lock_record(fd, offset, sizeof(User), F_UNLCK);
            offset += sizeof(User);
        }
        close(fd);
    }
    pthread_mutex_unlock(&users_mutex);
    printf("[DATABASE] Binary database initialized & all active session flags reset to 0.\n");
}
