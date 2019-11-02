CC=gcc
CFLAGS= -Wall
LDLIBS=-lrt -lpthread $(shell pkg-config --cflags --libs ncurses) 
BUILD_dir=build
WORK_dir=work
DEP=$(WORK_dir)/measure_mode.o $(WORK_dir)/sdaq_drv.o modes.h sdaq_drv.h main.c

$(BUILD_dir)/SDAQ_worker: $(DEP)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(WORK_dir)/measure_mode.o: measure_mode.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)
	
$(WORK_dir)/sdaq_drv.o: sdaq_drv.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)
	
tree: 
	mkdir -p $(BUILD_dir) $(WORK_dir)  

delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

.PHONY: all clean delete-the-tree 


