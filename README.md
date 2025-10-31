
# Bank Management System – Server-Client Architecture
## Usecases
- Customers can login to view and manage their bank accounts.
- Customers can perform transactions: deposit, withdraw, and transfer funds.
- Customers can apply for different types of loans and track their status.
- Customers can provide feedback and view transaction history.

- Employees can add new customers and modify customer details.
- Employees can view assigned loan applications and approve or reject them.

- Admins can add new employees and modify details of customers and employees.
- Admins have rights to manage user roles and data.

- Managers can activate or deactivate customer accounts.
- Managers can list all employees and manage loan assignments.
- Managers can review customer feedback and change their own password.
- All roles have dedicated menus and session handling for smooth operation.

## 🛠️ Tech Stack

** C-Programming


## Run Locally

Clone the project

```bash
  git clone https://github.com/adityadave29/BMS.git
```

Go to the project directory

```bash
  cd my-project
```
To start the server, This command will compile add the files listed in Makefile

```bash
  cd server
  make 
  make run 
  make clean
```

To run the client

```bash
  cd client
  gcc bm_client.c -o client
```


## Running Tests

To check Concurrancy and ACID propertie

```bash
   email: d@gmail.com
   password: 2901
   role: user
```
Now, if you try to log in with the same credentials from another terminal, the system will not allow it.
This ensures one active session per user at a time.

To achieve this, the system uses a file named d_active.txt, which maintains a list of currently logged-in users.

If the user session terminates unexpectedly (for example, by pressing Ctrl+C or Ctrl+Z), a signal handler is triggered.
This handler ignores the signal for 10 seconds and then automatically removes the user's email from d_active.txt to maintain consistency.

### Test 2
Now, consider a scenario where two employees are simultaneously adding new customers and both press the Add Customer button at the same time. In this situation, both processes access a shared file that provides sequential customer IDs.

To prevent a race condition, the system implements file locking.
This ensures that: The first employee gets the current customer ID. The next employee receives the incremented ID only after the first process completes.

The same locking logic is applied for:

- New employee creation
- Feedback entries
- Transaction IDs
- Loan IDs
