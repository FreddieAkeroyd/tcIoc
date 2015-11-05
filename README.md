TwinCAT-EPICS IOC
=================

The TwinCAT-EPICS IOC synchronizes TwinCAT variables with EPICS
channels. This software can be considered as an alternate to the
OPC-EPICS Gateway from HZB.

Further documentation is available at:

  https://dcc.ligo.org/LIGO-T1300690/public

Overview
--------

The IOC handles communication between two interfaces: TwinCAT and
EPICS. As such, it makes use of the TwinCAT ADS Communication Library
to communicate with the PLC on one side and the libraries of the EPICS
Base on the other side.

TwinCAT Communication
---------------------

The IOC keeps an internal database of the records that it
manages. This gets initialized through parsing the tpy file of a
TwinCAT project, which is generated every time the project is
built. This tpy file will provide the names and memory locations of
TwinCAT symbols on one PLC. The IOC uses this information to read and
write data via ADS commands to TwinCAT. Reading and writing are done
via two scanner threads, each executing a read or write command every
n ms, with n set by tcSetScanRate at IOC startup (see the example
st.cmd below). Multiple tpy files can be loaded in a single IOC,
i.e. the IOC can manage records on multiple PLCs.

EPICS Communication
-------------------

To notify EPICS that a record value has been updated, the IOC will
generate an interrupt or a callback request to EPICS, telling EPICS to
process that record. EPICS record processing includes grabbing the
value from the IOC's internal memory, checking alarms, processing
linked records, and updating channel access clients
(e.g. MEDM). Conversely, when EPICS receives a new value for a record
through channel access, it will process the record, and write the new
value to the IOC's internal memory.

Synchronization
---------------

The setup of the IOC's internal database is designed to prevent the
overlap of TwinCAT/EPICS read/write requests on the same record, which
could cause data values to become lost.

The internal data value has flags to indicate that the value has been
written by one side (TwinCAT or EPICS), but has not yet been read by
the other side. Thus, if EPICS has written a new value to the IOC,
TwinCAT cannot overwrite that value until it has read the old
value. This logic guarantees that data values do not get lost within
the IOC, that a value written on one side always makes it to the other
side, preventing collisions of write requests. We also prevent the
crossing of a read request with a write request by having the
read/write scanners on the TwinCAT side use mutexes when accessing the
IOC's database. Note that simultaneous reading of the IOC database
does not pose any problems, so this case is not prevented.

In addition, the IOC runs a scanner thread for each PLC that slowly
crawls through the internal database and pushes data values to
EPICS. It will cover the entire database once per minute, which
guarantees that the record values seen in EPICS/MEDM are never more
than a minute old.

Other Features
--------------

At startup, the IOC will parse tpy files to generate EPICS
databases. The user can specify which types of records are exported,
conversion rules between TwinCAT names and EPICS names, and whether to
split databases into multiple files. The user can also allow the
parser to generate other lists alongside the EPICS database, for
example channel lists and burt restore files. A full list of the
available options can be found on page TwinCAT EPICS Options.

Versions
--------

* Version 1.0: Initial release.
* Version 1.1

Added features:

* Add support for alias OPC properties to assign alternate names to
  symbols and structure elements.

* Add support for initial LIGO vacuum channels of the form
  HVE-EY:IP_...

Bug Fixes:

* Fixed severity for analog alarm values.
* Added additional messages for ADS errors.

Future Features
---------------

While the core functionality of the IOC is already in place and fully
tested, several additional features are being developed and will be
implemented soon:

* IOC status records: The idea is to keep track of things related to
  the IOC itself and export these to EPICS. Possible interesting
  figures are:
  * Average number of records updated per unit of time
  * Average amount of time per TwinCAT read/write request

Upgrades/Adaptations

* The code has enough abstraction that it can easily be adapted to
  work with other non-TwinCAT PLC controllers as well as other
  non-EPICS industrial control systems.

Code Documentation
------------------

The program was written in C++ and built using Visual Studio 2012, the
TwinCAT ADS Communication Library that comes with TwinCAT PLC
v2.11.1551, and EPICS Base version 3.14.12.3. Code documentation is
generated by Doxygen and can be found at this webpage:

Instructions for building the ioc from the source can be found at
TwinCAT EPICS IOC Source Build.


Running the IOC
===============

The startup requires several user inputs. A set of commands is given
as a parameter to tcIoc.exe which specifies these inputs. Below is an
example:

dbLoadDatabase("./tCat.dbd",0,0)             #load database definition file for TwinCAT device support
tCat_registerRecordDeviceDriver(pdbbase)     #register TwinCAT support with EPICS
callbackSetQueueSize(5000)                   #set the size of the EPICS callback buffer (default: 2000)
tcSetScanRate(10, 5)                         #set the time between requests to TwinCAT (10ms here)
                                             #and the slowdown multiple for pushing values to EPICS (5x here)
tcLoadRecords("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.tpy", "")    #load symbols from a tpy file
iocInit()                                    #initialize the IOC

A complete list of TwinCAT EPICS commands can be found on page TwinCAT
EPICS Commands.

To start the IOC,

* Install using the PowerShell script in install.ps1

* Run the start script start.bat 

On some systems it may be required to install the Visual C++
Redistributable for Visual Studio 2012

TwinCAT EPICS Commands
----------------------

The available commands are:

* tcSetAlias: Sets an alias name for a TwinCAT PLC. This name is used
  to define the info records. The alias name is applied when
  tcLoadRecords is called. It is reset afterwards.

  Example: tcSetAlias("C1PLC1")

  Sets the alias name to "C1PLC1".

* tcSetScanRate: Sets the scan rate for the TwinCAT PLC. The first
  argument is the scan rate in ms for the read or write scanners
  reading and writing TwinCAT variables. The second number is a
  multiple which describes the slow down for updating the EPICS
  read-only channels. The update rate for read/write channels is the
  same as the TwinCAT scan rate.

  Example: tcSetScanRate(10,5)

  Yields a 10ms TwinCAT update rate, and a 50ms EPICS update rate for read-only channels.

* tcGenerateList: Generates an additional listings when the records
  are loaded. Multiple tcList commands can be called in series to
  produce different listing. The first argument is a output file
  name. The second argument is a set of options. The lists are
  generated when tcLoadRecords is called. The list commands are reset
  afterwards.

  Example 1: tcGenerateList("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.req","-lb")

  This will generate an autoburt request file.

  Example 2: tcGenerateList("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.opc.txt","-l -rn -yi -cp")

  This will generate a listing of OPC names.

  Example 3: tcGenerateList("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.chn.txt","-l")

  This will generate a listing of EPICS names.

  Example 4: tcGenerateList("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.ini","-l -ns")

  This will generate an EPICS listing without string channels. The available options are listed on page TwinCAT EPICS Options.

* tcGenerateMacros: Generates ASCII macro lists (aml files) which can
  be used to generate ADL files for medm. The first argument is a
  output directory which is used to store the macro files. The second
  argument is a set of options. The macro files are generated when
  tcLoadRecords is called. The list commands are reset afterwards.

  Example 1: tcGenerateMacros("C:\SlowControls\Target\H1ECATX1\ADL")

  This will generate a macro files for each encountered structure
  including both fields and error messages. The resulting files are
  stored in the ADL subdirectory. Error messages require corresponding
  exp files (see the coding standard, E1200225.

  Example 2: tcGenerateMacros("C:\SlowControls\Target\H1ECATX1\ADL", "-mf")

  Generates macro files without error messages.

* tcLoadRecords: Loads a tpy file, then generates and loads the EPICS
  database. The first argument is the filename to the tpy file. The
  generated db file will have the same name but with the extension
  ".db". The second argument is a set of options.

  Example: tcLoadRecords("C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.tpy","")

  This command will parse the specified tpy file, then generate a db
  file with the name "C:\SlowControls\Target\H1ECATX1\PLC1\PLC1.db"
  and the specified options. The available options are listed on page
  TwinCAT EPICS Options.

The above commands will only be executed before iocInit() is
called. Multiple tpy files can be loaded by issuing multiple
tcLoadRecords commands. However, tcSetAlias and tcGenerateList need to
be specified anew before each tcLoadRecords command. The rate
specified with tcSetScanRate will be reused unless a new tcSetScanRate
command has been issued.

TwinCAT EPICS Options
---------------------

When generating a db file or a listing, a set of options describing
the conversion rules is available. Options can be specified either
Windows or Unix style. Meaning, both /ea and -ea will produce the same
result.

Channel Processing:

| /eo | Only export variables which are marked by an OPC export directive in the tpy file (default) |
| /ea | Export all variables regardless of the OPC settings in the tpy file |
| /ys | String variables are processes (default) |
| /ns | No string variables are processed |
| /pa | Process all types (default) |
| /ps | Process only simple types types, e.g., INT, BOOL, DWORD, etc. |
| /pc | Process only complex types, e.g., STRUCT, ARRAY |

Channel Name Conversion:

| /rl | LIGO standard conversion rule (default) |
| /rv | LIGO rules for initial vacuum channel names (version 1.1) |
| /rd | Replace dots with underscores in channel names |
| /rn | Do not apply any special conversion rules |
| /cp | Preserve case in EPICS channel names |
| /cu | Force upper case in EPICS channel names (default) |
| /cl | Force lower case in EPICS channel names |
| /nd | Eliminate leading dot in channel name (default) |
| /yd | Leave leading dot in channel name |
| /yi | Leave array indices in channel names |
| /ni | Replace array brackets with a single leading underscore (default) |

Split File Support:

| /nsio     | Do not split database or listing by record type (default) |
| /ysio     | Split database or listing into input only and input/ouput recrods |
| /sn 'num' | Split database or listing into files with no more than 'num' records |
| /sn 0     | Does not split database or listing into multiple files (default) |

Database Generation:

| /devopc | Use OPC name in INPUT/OUTPUT field (default) |
| /devtc  | Use TwinCAT name in INPUT/OUTPUT fields instead of OPC |

List Generation:

| /l  | Generate a standard listing, name only (default) |
| /ll | Generate a long listing, name and opc parameters |
| /lb | Generate an autoburt save/restore file |

Macro Generation:

| /ma | Generate a macro file for each structure describing fields and errors (default) |
| /me | Generate a macro file for each structure describing the error messages |
| /mf | Generate a macro file for each structure describing all fields |

Applicable options are:

| Program/Instruction | Available Options  | Enforced Options |
| tpyinfo             | channel processing | |
| EpicsDbGen          | all | |
| tcLoadRecords       | channel processing, channel name conversion | -ps -nsio -sn 0 -devtc |
| tcGenerateList      | channel processing, channel name conversion, list generation | -ps -nsio -sn 0 |
| tcGenerateMacros    | macro generation | |
| infoLoadRecords     | channel processing, channel name conversion | -ps -nsio -sn 0 -devtc
| infoGenerateList    | channel processing, channel name conversion, list generation | -ps -nsio -sn 0 |

Performance
-----------

Several checks on the performance of the IOC have been made to verify
that it will be able to reliably handle all ~20,000 Slow Controls
channels for extended periods of time.

Test system

Hardware:

    Processor: Intel Xeon CPU X5650
    Cores: 6 HT
    Threads: 12
    Speed: 2.67GHz
    Mmeory: 12 GB; 2.99GB usable 

Software:

    OS: Windows 7
    Version: 32-bit operating system
    TwinCAT: 2.11 

Speed tests

    TwinCAT (test performed on 6/21/2013)
    This test was performed to see how much data we can read from TwinCAT in one request before overloading the system.
        1 channel
            1.076ms to read data
            TwinCAT System Real Time Usage: was not monitored 
        1000 channels (~10kB)
            1.084ms to read data out in one request
            TwinCAT System Real Time Usage: no noticeable change 
        3,200 channels (~30kB)
            1.087ms to read data out in one request
            TwinCAT System Real Time Usage: +1-2% 
        7,500 channels (~70kB)
            1.099ms to read data out in one request
            TwinCAT System Real Time Usage: +3-4% 
        15,000 channels (~150kB)
            1.121ms to read data out in one request
            TwinCAT System Real Time Usage: +4-5% 

    TwinCAT (test performed on 6/20/2013)
    This test was performed to see how generating individual requests for each channel can overload the TwinCAT system. In this example we specified the memory location for each channel, instead of requesting one large memory region as above. This method proved to be too taxing on the TwinCAT system, so we do not use it in our IOC. Compare to the above performance figures.
        1000 channels
            1.306ms to get data for all channels
            TwinCAT System Real Time Usage: +20% 
        4000 channels
            1.483ms to get data for all channels
            TwinCAT System Real Time Usage: +60-80% 

    EPICS (test performed on 7/30/2013)
        It takes ~1.33s to process 1,000,000 records
        Thus in a 10ms cycle it can process ~7500 records 

Memory usage

(test performed on 8/24/2013)

    Running on the corner EtherCAT machine (H1ECATC1) (17000 records)
        IOC uses 40MB of memory
        6% of CPU time 
    Running on the end-X EtherCAT machine (H1ECATX1) (170 records)
        IOC uses 8.4MB of memory
        1.3% of CPU time 

High volume performance

(test performed on 8/9/2013)

    The IOC can safely handle a burt restore on the corner station, which restores all the EPICS records that are not read-only (~2000 in this case).
    The IOC can safely handle sequences of commands generated at a fast rate by the ezca tool. 

Performance over time

(test performed on 8/9/2013)

    The IOC has safely run for ~200 hours continuously on H1ECATC1 without any noticeable changes in memory usage or performance. 
