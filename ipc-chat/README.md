# Multi-Process Dialog System
## Message Exchange System with Shared Memory

**Operating Systems - Assignment 1**

**Period 2025-2026**

---

## Overview

This project implements a message exchange and broadcasting system between multiple processes using **POSIX shared memory** and **semaphore synchronization**. Processes can participate in real-time bidirectional communication through dialogs, where each process can simultaneously send and receive messages.

## Shared Memory

### Using mmap() and shm_open()

The system uses the `mmap()` and `shm_open()` functions to create a shared memory region accessible by all processes.

#### Memory Mapping Process

**First Process:**

1. Creates the shared memory segment with flags `O_CREAT | O_EXCL`
2. Sets the size with `ftruncate()` to match the size of the `shared_memory` struct
3. Maps the region into its address space with `mmap()`

**Subsequent Processes:**

1. Open the existing segment without the `O_EXCL` and `O_CREAT` flags
2. Map the same physical memory into their own address space
3. All processes now share the same memory region

### Detecting the First Process

To distinguish the first process from subsequent ones, we use the `O_CREAT | O_EXCL` flags:

- When the first process calls `shm_open()`, it succeeds and creates the segment
- When subsequent processes attempt with `O_EXCL`, it returns an error (the segment already exists)
- Based on this, we set the `rest_processes` and `first_process` flags accordingly

With `ftruncate()`, we set the shared memory size to be exactly the size in bytes of the `shared_memory` struct.

The `mmap()` function creates a new mapping in the virtual address space of the calling process. By setting `addr` to NULL, the kernel automatically selects a page-aligned address.

## Program Flow

When a process starts, it presents the user with the following options:

1. **Type 0:** Create a new dialog (the user specifies the max participant count)
2. **Type dialog ID:** Enter an existing dialog that appears in the terminal

After selecting a dialog, two threads are created using `pthread_create()`:

- **Writer thread:** Handles user input
- **Reader thread:** Monitors incoming messages

The main thread waits for both threads to complete with `pthread_join()`, then calls `cleanup()` and `munmap()` to remove the mapping and clean up resources.

## Threads

### Writer Thread

The writer thread handles user input and transmits messages to the shared memory buffer.

#### Implementation

1. Gains access to shared memory
2. While the `terminate` flag is disabled, continuously checks for input
3. Uses **poll()** for non-blocking input with timeout

#### Why poll();

We use `poll()` in combination with `fgets()` for non-blocking input:

- `poll()` checks if input is available with a 250ms timeout
- If there's no input, it returns and checks the `terminate` flag
- If input exists, `fgets()` reads it without blocking
- Without `poll()`, `fgets()` would block until Enter is pressed

#### Message Creation

1. Creates a new message struct
2. Initializes all fields (dialog_id, sender_id, payload)
3. **Locks the buffer** before writing (prevents race conditions)
4. Places the message in the shared memory buffer
5. Unlocks the buffer

**Why do we lock the buffer?**

If two processes from different dialogs write simultaneously without locking, both writer threads may write to the same buffer position, causing data corruption or message loss.

### Reader Thread

The reader thread continuously monitors shared memory for new messages.

**Why filter by dialog_id?**

Because the message buffer is shared by ALL dialogs, without this check, processes would see messages from completely unrelated conversations.

**The my_idx variable:**

`my_idx` is a global variable assigned during process initialization. Each process receives a unique index (0-19) used to track which messages the process has read in the `read_by[]` array.

#### Message Deactivation

After processing, the reader retrieves the participant count for the dialog. If `read_count >= participant_count - 1`, the message is deactivated.

**Why participant_count - 1?**

The sender does not read their own message (it's skipped), so we only expect `participant_count - 1` reads.

#### Handling Terminate

When the reader detects a "terminate" message:

1. It does not terminate immediately
2. Waits for all processes in the dialog to read the message
3. When everyone has read it, all processes terminate together

### cleanup()

Cleanup stage that executes after thread termination:

1. **Removal from process list:**
   - Sets `process_pids[my_idx] = 0`
   - Decrements `process_count`

2. **Removal from dialog participants:**
   - Searches through all dialogs
   - Finds the dialog to which this process belongs
   - Locates the process PID in the participant list

3. **Shifting remaining participants left:**
   - When a participant leaves, we shift all subsequent participants one position left
   - This keeps the array compact without gaps
   - Example: `[100, 200, 300]` → removal of 200 → `[100, 300, 0]`

4. **Dialog deactivation if empty:**
   - If `participant_count` reaches 0, the dialog is deactivated
   - The slot can be reused for future dialogs

5. **Final cleanup (only the last process):**
   - If `process_count == 0`, this is the last process
   - Destroys the semaphore
   - Unlinks the shared memory segment

## Execution Instructions

### Compilation

From the root directory, compile using:

```bash
make
```

To clean previous builds:

```bash
make clean
```

And then:

```bash
make
```

### Execution

#### Terminal 1 (First Process)

Split your terminal and execute:

```bash
./dialogue
```

You will see:

```
How many processes now are on? :1
Dialog selection: 
Enter dialog Id or 0 to create a new one
Available dialogs:
  No active dialogs found
```

This indicates you are the first process. Since there are no dialogs yet, you must create one.

Press `0` to create a new dialog:

```
Max participants:
3
Created a new dialog
Process 367215
```

A new dialog is created and your process ID is displayed.

#### Terminal 2+ (Subsequent Processes)

In another terminal, execute:

```bash
./dialogue
```

You will see:

```
How many processes now are on? :2
Dialog selection: 
Enter dialog Id or 0 to create a new one
Available dialogs:
  [1] Dialog: 1 (1/3 participants)
```

Now you can either:

- Press `0` to create a new dialog
- Press `1` to enter the existing dialog

**Important:** Don't always press `1`! Make sure you enter the correct dialog ID that appears in the list.

After entering:

```
1
Joined dialog : 1
Process 367436
```

### Termination

When any process types `terminate` in its terminal, **all processes in that dialog terminate automatically**.

### Cleaning Shared Memory in Case of Unexpected Termination

**Important Note:** If for any reason you interrupt the program with `Ctrl+C` or if an error occurs that prevents normal termination, the shared memory segment may remain in the system.

To manually clean shared memory, execute:

```bash
rm /dev/shm/*shared*
```

This command deletes all shared memory segments that contain the word "shared" in their name. Execute it only when you are sure that no program processes are still running.