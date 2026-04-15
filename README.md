# 💬 Shared Memory Chat System

A simple terminal-based chat application built in C using **shared memory**, **semaphores**, and **signals** for inter-process communication (IPC). Multiple clients can connect to a single server and chat in real time — all without using sockets or networking.

Built as a learning project to understand how processes communicate through shared memory on Linux.

## Contributors 
- Debabrata Goswami
- Atim Kumar Sasmal
---

## 🧠 How It Works

- The **server** creates a block of shared memory and manages connected clients.
- Each **client** attaches to that shared memory to read and send messages.
- Messages are stored in a **circular buffer** inside shared memory.
- **POSIX named semaphores** handle synchronization between processes.
- Each client uses `fork()` to split into a **reader** (listens for messages) and a **writer** (takes user input).

```
┌──────────┐     Shared Memory      ┌──────────┐
│  Client 1 │ ◄──────────────────► │  Server   │
└──────────┘    (messages + state)  └──────────┘
┌──────────┐          ▲
│  Client 2 │ ─────────┘
└──────────┘
```

---

## 📁 Project Structure

```
.
├── common.h      # Shared data structures and constants
├── server.c      # Server - manages shared memory and clients
├── client.c      # Client - connects, sends and receives messages
├── Makefile      # Build instructions
└── README.md     # You're reading this!
```

---

## 🛠️ Build & Run

### Prerequisites

- Linux (uses POSIX shared memory and semaphores)
- GCC compiler

### Compile

```bash
gcc -o server server.c -lpthread -lrt
gcc -o client client.c -lpthread -lrt
```

### Run

**Terminal 1 — Start the server:**
```bash
./server
```

**Terminal 2 — Start a client:**
```bash
./client
```

**Terminal 3 — Start another client:**
```bash
./client
```

---

## 💻 Commands

### Client Commands
| Command | Description          |
|---------|----------------------|
| `/list` | Show online users    |
| `/quit` | Disconnect from chat |
| `/help` | Show available commands |

### Server Commands
| Command | Description                  |
|---------|------------------------------|
| `list`  | Show connected clients       |
| `msgs`  | Show all messages            |
| `check` | Remove disconnected clients  |
| `quit`  | Shut down the server         |

---

## 📸 Demo

```
[Server] Starting...
[Server] Ready. Commands: list, msgs, check, quit

--- Client 1 ---
Enter your username: alice
[Client] Joined as 'alice'. Commands: /list /quit /help
you> hello everyone!

--- Client 2 ---
Enter your username: bob
[Client] Joined as 'bob'. Commands: /list /quit /help
[10:30:15] alice: hello everyone!
you> hey alice!
```

---

## ⚙️ Configuration

You can tweak these values in `common.h`:

| Constant        | Default | Description              |
|-----------------|---------|--------------------------|
| `MAX_CLIENTS`   | 10      | Max simultaneous clients |
| `MAX_MESSAGES`  | 50      | Message buffer size      |
| `MAX_MSG_LEN`   | 256     | Max message length       |
| `MAX_NAME_LEN`  | 32      | Max username length      |

---

## 🧩 Concepts Used

- Shared Memory (`shmget`, `shmat`, `shmdt`)
- POSIX Named Semaphores (`sem_open`, `sem_wait`, `sem_post`)
- Process Forking (`fork`)
- Signal Handling (`SIGINT`, `SIGUSR1`)
- Circular Buffer for message storage

---

## ⚠️ Limitations

- Linux only (not compatible with Windows/macOS)
- All clients must be on the same machine
- No message encryption
- No persistent message history

---

## 📝 License

This project is open source. Feel free to use, modify, and learn from it.
