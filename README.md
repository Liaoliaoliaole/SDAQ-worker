<![picture](./Docs/Artwork/Rview.png)>
# Wind station Datalogger

This repository related to hardware and software design of a Wind station Datalogger.
This device is capable to interface with two wind measurement sensors, a tachometer type for wind speed and a wind direction transmeter with parallel gray output. Also, provide the measurements to a set of analog indicators (uAmmeter and a desynn) and produce NMEA0183 logging messages through RS-232 serial line. 

## Getting Started

In the "Design and source" folder, can be found.
* [The Firmware source code](https://gitlab.com/fantomsam/wind_station/tree/master/Design_and_source/Firmware)
* [PCB Design Files](https://gitlab.com/fantomsam/wind_station/tree/master/Design_and_source/Hardware)

In the "Docs" folder, can be found.
* [The Technical Manual](https://gitlab.com/fantomsam/wind_station/raw/master/Docs/Documentation/Technical_manual.pdf?inline=false)
* [Useful Datasheets](https://gitlab.com/fantomsam/wind_station/tree/master/Docs/Datasheet)

### Prerequisites

For Firmware compilation the following toolkits are required

* [SDCC](http://sdcc.sourceforge.net/) - Small Device C Compiler suite (>=3.9.0)
* [GPUTILS](https://gputils.sourceforge.io/) - GPUTILS is a collection of tools for the PIC microcontrollers (>=1.5.2)
* [GNU Make](https://www.gnu.org/software/make/) - GNU make utility

### Compilation
To compile the firmware (tested under GNU/Linux only)
```
$ git clone https://gitlab.com/fantomsam/wind_station.git
$ cd wind_station/Design_and_source/Firmware
$ make tree
$ make
```
The binary file located under the build directory.

## Authors
* **Sam Harry Tzavaras** - *Initial work*

## License
The hardware PCB design is licensed under TAPRv1 or later - see the [License](https://gitlab.com/fantomsam/wind_station/raw/master/Design_and_source/Hardware/TAPR_Open_Hardware_License_v1.0.pdf?inline=false) file for details.

The source code of the firmware is licensed under GPLv3 or later - see the [License](License) file for details.

The Technical manual is licensed under FDLv1.3 or later - see the [License](https://gitlab.com/fantomsam/wind_station/raw/master/Docs/Documentation/fdl-1.3.pdf?inline=false) file for details.
