# Multi-Process Dialog System

A message exchange system between multiple processes using POSIX shared memory and semaphores. Processes can join dialogs and communicate in real-time.

## Features

- Shared memory IPC using `shm_open()` and `mmap()`
- Semaphore-based synchronization for thread safety
- Separate reader/writer threads per process
- Support for multiple concurrent dialogs
- Automatic resource cleanup on exit

## Build

```bash
make
```

To rebuild:

```bash
make clean && make
```

## Usage

Run `./dialogue` in multiple terminals. Each process can:

- Enter `0` to create a new dialog (specify max participants)
- Enter a dialog ID to join an existing dialog

Type messages to send them to all participants in your dialog. Type `terminate` to end the dialog for all participants.

## Cleanup

If the program is interrupted (`Ctrl+C`), shared memory may persist. Clean it manually:

```bash
rm /dev/shm/*shared*
```