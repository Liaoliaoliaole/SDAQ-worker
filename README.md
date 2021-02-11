# Project SDAQ_worker

## Preamp
This repository related to a software control and emulate suite related to SDAQ devices. An SDAQs is a proprietary (developed by iCraft Oy) acquisition devices with predefined physical input (voltage, current, thermocouple, etc) and CANBus interface as output.

The SDAQ_worker project was started with the philosophy to make some software that can control this devices from a computer that is equip with CAN interface (Linux Socket CAN compatible) and runs GNU operating system.

## Executables
After the compilation two executable files produced:
* [SDAQ_worker](#usage-sdaq_worker)
* [SDAQ_psim](#usage-sdaq_psim)

The SDAQ_worker is the SDAQ manipulation/controlling software.<br>
The SDAQ_psim is a SDAQ software emulator.

### Requirements
For compilation of this project the following dependencies are required.
* [GCC](https://gcc.gnu.org/) - The GNU Compilers Collection
* [GNU Make](https://www.gnu.org/software/make/) - GNU make utility
* [NCURSES](https://www.gnu.org/software/ncurses/ncurses.html) - A free (libre) software emulation library of curses.
* [GLib](https://wiki.gnome.org/Projects/GLib) - GNOME core application building blocks libraries.
* [libxml2](http://xmlsoft.org/) - Library for parsing XML documents
* [zlib](https://www.zlib.net/zlib_how.html) - A free software library used for data compression.

##### Optionally
* [CAN-Utils](https://elinux.org/Can-utils) - CANBus utilities

### Compilation
To compile the programs (tested under GNU/Linux only)
```
$ # Clone the project's source code
$ git clone https://gitlab.com/fantomsam/sdaq-worker.git
$ cd sdaq_worker
$ # Make the compilation directory tree
$ make tree
$ make
```
The executable binaries located under the **./build** directory.

### Installation
```
$ sudo make install
```
### Un-installation
```
$ sudo make uninstall
```

### Usage: SDAQ_worker
```
Usage: SDAQ_worker CAN-IF MODE [ADDRESS] [SERIAL NUMBER] [LOGGING DIRECTOR] [Options]

CAN-IF: The name of the CAN-Bus adapter

MODE:
      discover: Discovering the connected SDAQs.

    autoconfig: Set valid address to all Parked SDAQs.

    setaddress: Change the address of a SDAQ.
                (Usage: SDAQ_worker CAN-IF setaddress 'new_address' 'Serial_number_of_SDAQ')
       getinfo: Get all the available information of a SDAQ device.
                (Usage: SDAQ_worker CAN-IF getinfo 'SDAQ_address')
       setinfo: Set the Calibration data and points information on a SDAQ device.
                (Usage: SDAQ_worker CAN-IF setinfo 'SDAQ_address')
       measure: Get the measurements, status and info of a SDAQ device.
                (Usage: SDAQ_worker CAN-IF measure 'SDAQ_address')
       logging: Get and log the measurement of a SDAQ device to a file.
                (Usage: SDAQ_worker CAN-IF logging 'SDAQ_address' 'Path/to/the/logging_directory')

ADDRESS: A valid SDAQ address. Resolution 1..62 (also 'Parking' for Mode 'setaddress')

Options:
           -h : Print help.
           -V : Version.
           -s : Silent print, or with mode 'getinfo' print info at stdout in XML format
           -r : resize terminal. Used with mode 'measure'
           -v : Address Verification. Used with mode 'setaddress'.
           -l : Print a list of the available CAN-IFs.
           -f : Write/Read SDAQ info to/from file.
           -p : Formatted XML output. Used with mode 'getinfo'.
           -e : External command. Used with mode 'setinfo'.
  -t <Timeout>: Discover Timeout (sec). (0 < Timeout < 20) default: 2 Sec.
  -S <Mode>   : Timestamp mode. (A)bsolute/(R)elative/(D)ate.
  -T <format> : Timestamp format, works with -S Date.
```
### Usage: SDAQ_psim
```
Usage: SDAQ_psim CAN-IF Num_of_pSDAQ [Options]

	CAN-IF: The name of the CAN-Bus interface

	Num_of_pSDAQ: The number of the pseudo_SDAQ devices, Range 1..62

	Options:
	         -h : Print Help
	         -v : Print Version
	         -l : Print list of CAN-IFs
	         -s : S/N of the first pseudo_SDAQ. (Default 1)
	         -c : Initial Amount of channels of each pseudo_SDAQ, (default 1, Range:[1-16])

			      -----SDAQ_psim Shell-----

 KEYS:  KEY_UP    = Buffer up
		KEY_DOWN  = Buffer Down
		KEY_LEFT  = Cursor move left by 1
		KEY_RIGTH = Cursor move Right by 1
		Ctrl + C  = Clear current buffer
		Ctrl + L  = Clear screen
		Ctrl + I  = print used CAN-if
		Ctrl + Q  = Quit
 COMMANDS:
		code #(-#) = print the unit string of the code (or range of codes)
		status (S/N) = Print status of S/N or all pSDAQs without S/N
		status S/N CH# = Print calibration points status of CH# at pSDAQ with S/N
		get (S/N) = Get the current outputs state
		set (S/N) on/off = Set a pseudo-SDAQ on or off line
		set (S/N) address (# || parking) = Set pSDAQ's address
		set (S/N) amount = Set the amount of channels. Range 1..16
		set (S/N) (ch# || all) [no]noise = [Re]Set random noise on channel(s)
		set (S/N) (ch# || all) [no]sensor = [Re]Set No sensor flag(s)
		set (S/N) (ch# || all) (out || in) = [Re]Set "out of range" flag(s)
		set (S/N) (ch# || all) (over || under) = [Re]Set "Over-Range" flag(s)
		set (S/N) (ch# || all) Real_val = Write value to Channel(s) output
		set (S/N) (ch# || all) date (now || YYYY/MM/DD) = Load Calibration Date
		set (S/N) (ch# || all) points # = Load Amount of Calibration points
		set (S/N) (ch# || all) period # = Write Calibration period
		set (S/N) (ch# || all) unit # = Load unit code
		set (S/N) ch# p(oint)# name Real_val = Set Channel's point value
        		name := Meas, Ref, Offset, Gain, C2, C3
```

## Examples
```
$ # Load Virtual-CANBus module to Kernel
$ sudo modprobe vcan
$ # Make a new network device with name 'vcan0' and type 'vcan'
$ sudo ip link add dev vcan0 type vcan
$ sudo ip link set up vcan0
```
###### Throw 10 pseudo_SDAQ on the vitual CANBus "vcan0".
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
1. ~~User Interface~~

## Authors
* **Sam Harry Tzavaras** - *Initial work*

## License
The source code of the SDAQ_worker project is licensed under GPLv3 or later - see the [License](LICENSE) file for details.
##### Avatar
Icons by [Icons8](http://icons8.com)
