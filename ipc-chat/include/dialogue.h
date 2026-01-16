#pragma once
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "defines.h"
#include <stdbool.h>

/*
a dialog struct that keeps the dialogue_id,
all the participant ids
how many participant are in the dialogue
and if the dialogue is active.

*/

typedef struct Dialogue{
    int dialog_id; 
    pid_t participant_ids[MAX_PROCS];
    int participant_count;
    int max_participants;
    bool is_active;
}Dialogue;