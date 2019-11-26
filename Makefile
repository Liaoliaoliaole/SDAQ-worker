CC=gcc
CFLAGS= -std=c99 -Wall
LDLIBS=-lrt -lpthread $(shell pkg-config --cflags --libs ncurses glib-2.0 libxml-2.0)
BUILD_dir=build
WORK_dir=work
SRC_dir=src
DEP=$(WORK_dir)/Discover_and_autoconfig.o \
    $(WORK_dir)/Measure.o $(WORK_dir)/Logging.o \
    $(WORK_dir)/info.o $(WORK_dir)/SDAQ_drv.o \
    $(WORK_dir)/SDAQ_xml.o \
   $(WORK_dir)/SDAQ_psim_UI.o

all: $(BUILD_dir)/SDAQ_worker $(BUILD_dir)/SDAQ_psim

$(BUILD_dir)/SDAQ_worker: $(DEP) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_worker.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_dir)/SDAQ_psim: $(DEP) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_psim.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

#Dependentes for binaries
$(WORK_dir)/Discover_and_autoconfig.o: $(SRC_dir)/Discover_and_autoconfig.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Measure.o: $(SRC_dir)/Measure.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Logging.o: $(SRC_dir)/Logging.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/info.o: $(SRC_dir)/info.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/SDAQ_drv.o: $(SRC_dir)/SDAQ_drv.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/SDAQ_xml.o: $(SRC_dir)/SDAQ_xml.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/SDAQ_psim_UI.o: $(SRC_dir)/SDAQ_psim_UI.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

tree:
	mkdir -p $(BUILD_dir) $(WORK_dir)

delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

.PHONY: all clean delete-the-tree tree


