#pragma once
#include "dialogue.h"
#include "defines.h"
/*
this struct has a unique dialog id that the starting process will init
a sender_id to know who sent it
the payload(the message actually)
read_count->how many processes read this message
read_by->to know if the process has already read the message so it doesn't read it again
is_active->if the message is read by all the processes then i deactivate it
so i can use the slot again
*/

typedef struct Message{
    int dialog_id;
    pid_t sender_id;
    char payload[256];
    int read_count;
    bool read_by[MAX_PROCS];
    bool is_active;
}Message;