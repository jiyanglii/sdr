INC_DIR	= ./include
SRC_DIR = ./src
OBJ_DIR	= ./object

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(SRC_FILES:.c=.o)
OBJ_PATH  = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(OBJ_FILES))

LIBS	=
CC	= clang
CFLAGS	= -g -I$(INC_DIR) -D_FORTIFY_SOURCE=2 -Werror=implicit-function-declaration -Werror=format-security -Wpedantic -Werror -Wshadow -Wstrict-overflow -fno-strict-aliasing -Wno-missing-field-initializers -O2 -march=native

all: assignment3

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

assignment3: $(OBJ_PATH)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f $(OBJ_DIR)/*.o $(INC_DIR)/*~ assignment3
