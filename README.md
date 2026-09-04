# Bank Management System – Server-Client Architecture

A high-performance, multithreaded Bank Management System written in C using TCP socket programming, fixed-size binary records (`.dat`), record-level `fcntl` byte-range locking for ACID compliance, and role-based access control.

---

## 📁 Project Structure

```text
BMS/
├── client/
│   ├── Makefile                  # Build instructions for client
│   └── src/
│       └── client.c              # Client CLI interface and signal handling
├── server/
│   ├── Makefile                  # Build instructions for server
│   ├── data/                     # Fixed-size binary database engine (.dat)
│   │   ├── users.dat             # User credentials, roles & is_logged_in flag
│   │   ├── customers.dat         # Customer profile details & account info
│   │   ├── employees.dat         # Employee & Manager profile records
│   │   ├── balance.dat           # Account balances (in-place byte locking)
│   │   ├── loans.dat             # Loan records & assignment statuses
│   │   ├── transactions.dat      # Append-only transaction ledger
│   │   ├── feedbacks.dat         # Customer feedback records
│   │   └── counters.dat          # Atomic sequential ID generator counters
│   ├── include/
│   │   └── server.h              # Struct definitions, constants & DB prototypes
│   └── src/
│       ├── main.c                # Multithreaded TCP server (pthread listener)
│       ├── db.c                  # Binary database engine & fcntl record locking
│       ├── handle_client.c       # Client connection worker thread
│       ├── auth.c                # Authentication & in-place session locking
│       ├── transaction.c         # Deposits, withdrawals, transfers & ledger
│       ├── client_menu.c         # Customer menu operations & password change
│       ├── admin_menu.c          # Admin operations (employee/customer onboarding)
│       ├── employee_menu.c       # Employee operations (customer creation, loans)
│       └── manager_menu.c        # Manager operations (activation, assignments)
├── docs/
│   ├── diagram.png               # System architecture diagram
│   └── diagram.xml               # Diagram source XML
├── Makefile                      # Top-level orchestrator Makefile
├── .gitignore                    # Git ignore file for binaries and temporary files
└── README.md                     # Project documentation
```

---

## 🚀 Use Cases & Roles

- **Customers**:
  - View balance and deposit/withdraw/transfer funds with atomic locking.
  - Apply for loans and track approval status.
  - Submit feedback and view personal transaction history.
  - Change account password in-place.

- **Employees**:
  - Add new customers and modify customer details.
  - View assigned loan applications and approve or reject them with automatic balance crediting.

- **Admins**:
  - Add new employees and modify employee/customer profiles.
  - Manage user roles and system records.

- **Managers**:
  - Activate or deactivate customer accounts (`is_active = 1/0`).
  - List all employees and assign loans to specific employees.
  - Review customer feedback and change manager password.

---

## 🛠️ Tech Stack & Key Architectures

- **Language**: C (C99/C11)
- **Networking**: POSIX TCP Sockets (`sys/socket.h`, `netinet/in.h`)
- **Concurrency**: POSIX Threads (`pthread`), detached worker model
- **Database Engine**: Fixed-size binary structs with direct seek & in-place update
- **Locking & ACID**: POSIX Record-Level Byte-Range Locking (`fcntl` with `F_RDLCK`, `F_WRLCK`)
- **Build System**: GNU Make

---

## ⚡ Quick Start & Build

### 1. Build Both Server and Client
From the project root:
```bash
make
```

### 2. Run the Server
```bash
make run-server
# or
cd server && make run
```

### 3. Run the Client
In another terminal window:
```bash
make run-client
# or
cd client && make run
```

### 4. Clean Build Artifacts
```bash
make clean
```

---

## 🧪 Concurrency, Locking & Session Features

### 1. Record-Level `fcntl` Byte-Range Locking
- Every record is a fixed-size C struct (`User`, `Customer`, `AccountBalance`, etc.).
- When reading or updating a record at index $N$, the server calculates `offset = N * sizeof(Struct)` and applies `fcntl` record locking (`F_RDLCK` or `F_WRLCK`) on **only those specific bytes**.
- No temporary files (`temp_*.txt`) are created; updates occur directly in-place with zero file re-writing overhead.

### 2. Integrated `is_logged_in` Flag & Crash Recovery
- Single active session per user is enforced directly within `users.dat` using the `is_logged_in` boolean flag.
- When a user logs in, their `is_logged_in` field is set to `1` using a write lock.
- Duplicate login attempts from another terminal are blocked.
- On server startup, `init_database()` automatically resets `is_logged_in = 0` across all records, ensuring 100% crash resilience without stale lockouts.
