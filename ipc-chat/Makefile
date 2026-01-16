CC = gcc
CFLAGS = -g -Wall -Iinclude
LDFLAGS = -g
LDLIBS = -lrt -lpthread

SRC_DIR = src
OBJ_DIR = obj

DIALOGUE_SRC = $(SRC_DIR)/dialogue.c
DIALOGUE_OBJ = $(OBJ_DIR)/dialogue.o
DIALOGUE     = $(SRC_DIR)/dialogue

all: $(OBJ_DIR) $(DIALOGUE)
dialogue: $(DIALOGUE)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(DIALOGUE): $(DIALOGUE_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -rf $(OBJ_DIR) $(DIALOGUE)

.PHONY: all dialogue clean