CC=gcc
CFLAGS= -Wall
LDLIBS=-lrt -lpthread $(shell pkg-config --cflags --libs ncurses glib-2.0 libxml-2.0) 
BUILD_dir=build
WORK_dir=work
SRC_dir=src
DEP=$(WORK_dir)/Discover_and_autoconfig.o $(WORK_dir)/Measure.o $(WORK_dir)/Logging.o $(WORK_dir)/Dev_info.o $(WORK_dir)/SDAQ_drv.o

all: $(BUILD_dir)/SDAQ_worker $(BUILD_dir)/SDAQ_psim

$(BUILD_dir)/SDAQ_worker: $(DEP) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_worker.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_dir)/SDAQ_psim: $(DEP) $(SRC_dir)/SDAQ_drv.h $(SRC_dir)/SDAQ_psim.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(WORK_dir)/Discover_and_autoconfig.o: $(SRC_dir)/Discover_and_autoconfig.c 
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

.PHONY: all clean delete-the-tree tree 


