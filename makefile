
SRC=sfs_disk.c sfs_main.c
LIB= sfs_func_ext.o

mysfs: sfs_func_hw.c
	$(CC) -o $@ $(SRC) $< $(LIB)

clean:
	$(RM) mysfs sfs_func_hw.o
