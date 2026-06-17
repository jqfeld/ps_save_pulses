CC = gcc
LD_FLAGS = -lps5000a -lm -lhdf5

rapid_block_mode: rapid_block_mode.c
	$(CC) $? -I/opt/picoscope/include -L/opt/picoscope/lib $(LD_FLAGS) -o $@

