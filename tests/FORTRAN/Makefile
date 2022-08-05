CC=mpif90

PASS_DIR=../../build/

BC_DIR=./bc
SRC_DIR=.

CFLAGS +=-g 

SRC_TEST= $(wildcard $(SRC_DIR)/*.c)
BC_TEST=$(SRC_TEST:.c=.bc)

all: $(BC_TEST)

%.bc: $(SRC_DIR)/%.c
	$(CC) -c -emit-llvm $(CFLAGS) -o $(BC_DIR)/$@ $<
	$(PASS_DIR)/lcdBC $(BC_DIR)/$@

clean:
	rm -rf $(BC_DIR)/*.bc
