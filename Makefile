CC=gcc
CFLAGS= -Wall
LDLIBS=-lrt -lpthread $(shell pkg-config --cflags --libs ncurses) 
BUILD_dir=build
WORK_dir=work
SRC_dir=src
DEP=$(WORK_dir)/Discover.o $(WORK_dir)/Autoconf.o $(WORK_dir)/Change_address.o $(WORK_dir)/Measure.o $(WORK_dir)/Logging.o $(WORK_dir)/Dev_info.o $(WORK_dir)/SDAQ_drv.o

$(BUILD_dir)/SDAQ_worker: $(DEP) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_worker.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(WORK_dir)/Discover.o: $(SRC_dir)/Discover.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Autoconf.o: $(SRC_dir)/Autoconf.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Change_address.o: $(SRC_dir)/Change_address.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Measure.o: $(SRC_dir)/Measure.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Logging.o: $(SRC_dir)/Logging.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Dev_info.o: $(SRC_dir)/Dev_info.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)
	
$(WORK_dir)/SDAQ_drv.o: $(SRC_dir)/SDAQ_drv.c 
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)
	
tree: 
	mkdir -p $(BUILD_dir) $(WORK_dir)  

delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

.PHONY: all clean delete-the-tree 


