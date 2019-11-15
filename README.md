# SDAQ_worker Project

## Preamp
This repository related to a software control and emulate suite related to SDAQ devices. An SDAQs is a proprietary acquisition device (developed by iCraft Oy) with predefined physical input (voltage, current, thermocouple, etc) and CANBus output.  

The SDAQ_worker project was started with the phylosophy to make some softwares that can control this devices from a computer that is equip with CAN interface (Linux Socket CAN compatible) and runs GNU operating system.

## Executables
After the compilation 2 executable files are produced that are:
* SDAQ_worker
* SDAQ_psim

The SDAQ_worker is the SDAQ manipulation/controlling software.<br>
The SDAQ_psim is a SDAQ software emulator.  

### Prerequisites
For compilation of this project the following dependences are required.  
* [GCC](https://gcc.gnu.org/)-The GNU Compiler Collection
* [GNU Make](https://www.gnu.org/software/make/) - GNU make utility
* [NCURSES](https://www.gnu.org/software/ncurses/ncurses.html) - A a free software emulation library of curses.
* [GLib](https://wiki.gnome.org/Projects/GLib) - GNOME core application building blocks libraries.
* [libxml2](http://xmlsoft.org/) -  Library for parsing XML documents
##### Optionaly
* [can-utils](https://elinux.org/Can-utils) - CANBus utilities


### Compilation
To compile the firmware (tested under GNU/Linux only)
```
# Clone the project's source code
$ git clone https://gitlab.com/fantomsam/sdaq-worker.git
$ cd sdaq-worker
# Make the compilation directory tree
$ make tree
# Compile via Make
$ make
```
The binary file located under the build directory.

### Usage of SDAQ_worker
```
Usage: SDAQ_worker CAN-IF MODE [ADDRESS] [SERIAL NUMBER] [LOGGING DIRECTOR] [Options]

CAN-IF: The name of the CAN-Bus adapter

MODE:
      discover: Discovering the connected SDAQs.

    autoconfig: Set valid address to all Parked SDAQs.

    setaddress: Change the address of a SDAQ.
                (Usage: SDAQ_worker CAN-IF setaddress 'new_address' 'Serial_number_of_SDAQ')
       getinfo: Get all the available information of a SDAQ device.
       setinfo: Set the Calibration data and points information on a SDAQ device.
                (Usage: SDAQ_worker CAN-IF getinfo 'SDAQ_address')
       measure: Get the measurements, status and info of a SDAQ device.
                (Usage: SDAQ_worker CAN-IF measure 'SDAQ_address')
       logging: Get and log the measurement of a SDAQ device to a file.
                (Usage: SDAQ_worker CAN-IF logging 'SDAQ_address' 'Path/to/the/logging_directory')

ADDRESS: A valid SDAQ address. Resolution 1..62 (also 'Parking' if Mode 'setaddress')

Options:
           -h : Print help.
           -V : Version.
           -s : Silent print, or with mode 'getinfo' print info at stdout in XML format
           -v : Address Verification. Used with mode 'setaddress'.
           -l : Print a list of the available CAN-IF.
           -f : Write SDAQ info. Used with mode 'getinfo'
  -t <Timeout>: Discover Timeout (sec). (0 < Timeout < 20) default: 2 Sec
  -S <Mode>   : Timestamp mode. (A)bsolute/(R)elative/(D)ate.
  -T <format> : Timestamp format, works with -S Date.
```
### Usage of SDAQ_psim
```
Usage: SDAQ_psim CAN-IF [Amount of pseudo_SDAQ Devices]
```

## Examples
```
$ #Load virtual can module to Kernel
$ sudo modprobe vcan
$ #Make a new network device with name 'vcan0' and type 'vcan'
$ sudo ip link add dev vcan0 type vcan
$ sudo ip link set up vcan0
```
###### Throw 10 pseudo_SDAQ on the vitual CANBus vcan0.
```
$ SDAQ_psim vcan0 10
```

###### Discover the available SDAQs on "vcan0".
```
$ SDAQ_worker vcan0 discover
```
###### Autoconfig the available Parked SDAQs on "vcan0".
```
$ SDAQ_worker vcan0 autoconfig
```
###### Get measurements from SDAQ with address '1'.
```
$ SDAQ_worker vcan0 measure 1
```
#### TODO-list SDAQ_worker
##### Modes
1. ~~'discover'~~
2. ~~'autoconfig'~~
3. ~~'setaddress'~~
5. ~~'measure'~~
4. ~~'getinfo'~~
6. 'setinfo'
7. 'logging'

#### TODO-list SDAQ_psim
1. User Interface

## Authors
* **Sam Harry Tzavaras** - *Initial work*

## License
The source code of the SDAQ_worker project is licensed under GPLv3 or later - see the [License](License) file for details.
