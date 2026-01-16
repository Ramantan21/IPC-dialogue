#pragma once
#include "defines.h"
#include "dialogue.h"
#include "message.h"

typedef struct{
    Dialogue dialogs[MAX_DIALOGS]; // keeps the id of the dialogs in shm
    int active_dialog_count; // how many dialogs now are active
    Message messages[MAX_MESSAGES]; // the buffer
    int message_head;
    int message_count;
    int process_count;
    sem_t sem1; // semaphore to lock and unlock the buffer. so a thread can't write concurrently with another thread
    int process_pids[MAX_PROCS];
    int next_dialog_id;
}shared_memory;