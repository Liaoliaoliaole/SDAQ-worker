CC=gcc -O3
CFLAGS= -std=c99 -Wall #-g3 #-Wextra
LDLIBS= -lrt -lpthread $(shell pkg-config --cflags --libs ncurses glib-2.0 libxml-2.0 zlib)
D_opt = -D RELEASE_HASH='"$(shell git log -1 --format=%h)"' \
		-D RELEASE_DATE=$(shell git log -1 --format=%ct) \
        -D COMPILE_DATE=$(shell date +%s)
BUILD_dir=build
WORK_dir=work
SRC_dir=src
DEPs_SDAQ_worker=$(WORK_dir)/Discover_and_autoconfig.o \
				 $(WORK_dir)/Measure.o $(WORK_dir)/Logging.o \
				 $(WORK_dir)/getinfo.o $(WORK_dir)/setinfo.o\
				 $(WORK_dir)/SDAQ_drv.o \
				 $(WORK_dir)/SDAQ_xml.o \
				 $(WORK_dir)/SDAQ_psim_UI.o \
				 $(WORK_dir)/CANif_discovery.o \
				 $(WORK_dir)/ver.o

DEPs_SDAQ_psim=$(WORK_dir)/SDAQ_drv.o \
			   $(WORK_dir)/SDAQ_psim_UI.o \
			   $(WORK_dir)/CANif_discovery.o \
			   $(WORK_dir)/ver.o

DEPs_SDAQ_prog=$(WORK_dir)/SDAQ_drv.o \
			   $(WORK_dir)/CANif_discovery.o \
			   $(WORK_dir)/iHEX.o \
			   $(WORK_dir)/ver.o

TEST_SRC=$(SRC_dir)/SDAQ_xml.c $(SRC_dir)/getinfo.c $(SRC_dir)/setinfo.c $(SRC_dir)/SDAQ_drv.c
TEST_BIN=$(BUILD_dir)/test_calibration
TEST_ASAN_BIN=$(BUILD_dir)/test_calibration_asan
TEST_FLAGS=-O0 -g -std=c99 -Wall -Wextra
ASAN_DETECT_LEAKS?=1

all: $(BUILD_dir)/SDAQ_worker $(BUILD_dir)/SDAQ_psim $(BUILD_dir)/SDAQ_prog
install:install-SDAQ_worker install-SDAQ_psim install-SDAQ_prog

test: tree $(TEST_BIN)
	G_SLICE=debug-blocks $(TEST_BIN)

test-asan: tree
	$(CC) $(TEST_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_calibration.c $(TEST_SRC) -o $(TEST_ASAN_BIN) $(LDLIBS)
	G_SLICE=always-malloc ASAN_OPTIONS=detect_leaks=$(ASAN_DETECT_LEAKS):halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(TEST_ASAN_BIN)

$(TEST_BIN): tests/test_calibration.c $(TEST_SRC) $(SRC_dir)/*.h
	$(CC) $(TEST_FLAGS) tests/test_calibration.c $(TEST_SRC) -o $@ $(LDLIBS)

$(BUILD_dir)/SDAQ_worker: $(DEPs_SDAQ_worker) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_worker.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_dir)/SDAQ_psim: $(DEPs_SDAQ_psim) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_psim.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_dir)/SDAQ_prog: $(DEPs_SDAQ_prog) $(SRC_dir)/*.h $(SRC_dir)/SDAQ_prog/*.h $(SRC_dir)/SDAQ_prog/SDAQ_prog.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

#Dependents for binaries
$(WORK_dir)/SDAQ_drv.o: $(SRC_dir)/SDAQ_drv.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Discover_and_autoconfig.o: $(SRC_dir)/Discover_and_autoconfig.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Measure.o: $(SRC_dir)/Measure.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Logging.o: $(SRC_dir)/Logging.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/getinfo.o: $(SRC_dir)/getinfo.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/setinfo.o: $(SRC_dir)/setinfo.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/CANif_discovery.o: $(SRC_dir)/CANif_discovery.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/SDAQ_xml.o: $(SRC_dir)/SDAQ_xml.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/iHEX.o: $(SRC_dir)/SDAQ_prog/iHEX.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/SDAQ_psim_UI.o: $(SRC_dir)/SDAQ_psim_UI.c
	$(CC) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/ver.o: $(SRC_dir)/ver.c
	$(CC) $(D_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)


tree:
	mkdir -p $(BUILD_dir) $(WORK_dir)
delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)
clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*
install-SDAQ_worker:
	@echo "\nInstallation of SDAQ_worker..."
	install $(BUILD_dir)/SDAQ_worker -t /usr/local/bin/
	install $(SRC_dir)/autocomplete/SDAQ_worker -t /usr/share/bash-completion/completions/
install-SDAQ_psim:
	@echo "\nInstallation of SDAQ_psim..."
	install $(BUILD_dir)/SDAQ_psim -t /usr/local/bin/
	install $(SRC_dir)/autocomplete/SDAQ_psim -t /usr/share/bash-completion/completions/
install-SDAQ_prog:
	@echo "\nInstallation of SDAQ_prog..."
	install $(BUILD_dir)/SDAQ_prog -t /usr/local/bin/
	install $(SRC_dir)/autocomplete/SDAQ_prog -t /usr/share/bash-completion/completions/
install-manuals:
	install ./man_pages/SDAQ_worker.1 -t /usr/share/man/man1/
	install ./man_pages/SDAQ_psim.1 -t /usr/share/man/man1/
	install ./man_pages/SDAQ_prog.1 -t /usr/share/man/man1/
	sudo mandb
uninstall:
ifeq ($(shell ls /usr/local/bin/SDAQ* > /dev/null 2>&1; echo $$?), 0)
	@echo "Uninstall SDAQ_worker's binaries..."
	@rm /usr/local/bin/SDAQ_*
endif
ifeq ($(shell ls /usr/share/bash-completion/completions/SDAQ* > /dev/null 2>&1; echo $$?), 0)
	@echo "Uninstall SDAQ_worker's completion scripts..."
	@rm /usr/share/bash-completion/completions/SDAQ*
endif
ifeq ($(shell ls /usr/share/man/man1/SDAQ* > /dev/null 2>&1; echo $$?), 0)
	@echo "Uninstall SDAQ_worker's manuals..."
	@rm /usr/share/man/man1/SDAQ* && sudo mandb
endif
.PHONY: all clean delete-the-tree tree test test-asan install-SDAQ_worker install-SDAQ_psim install-SDAQ_prog
