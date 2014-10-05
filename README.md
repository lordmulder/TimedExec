TimedExec
=========

**TimedExec** is a small utility for *benchmarking* command-line programs. It will execute the specified command and measure the time it takes for that command to complete. All measurements are done via *high-resolution* performance timers. In order to obtain more accurate results, the each command will be executed *multiple* times. The number of runs can be configured as needed. TimedExec will then compute the *average* execution time, as well as the *fastest* and *slowest* execution time. Furthermore, the *standard deviation* of all runs will be computed. Last but not least, an optional number "warm-up" runs will be executed before the first measured run.


Command-line Usage
------------------

```
===========================================================================
Timed Exec - Benchmarking Utility
Copyright (c) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License <http://www.gnu.org/>.
Note that this program is distributed with ABSOLUTELY NO WARRANTY.
===========================================================================

Usage:
  TimedExec.exe <Program.exe> [Arguments]

Influential environment variables:
  TIMED_EXEC_LOGFILE - Log File (default: "TimedExec.log")
  TIMED_EXEC_LOOPS   - Number of execution loops (default: 5)
  TIMED_WARMUP_LOOPS - Number of warm-up loops (default: 1)
```
