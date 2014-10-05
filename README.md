TimedExec
=========

**TimedExec** is a small utility for *benchmarking* command-line programs. It will execute the specified command and measure the time it takes for that command to complete. In order to obtain more accurate results, all measurements are done via *high-resolution* performance timers. Also, in order to filter out environmental noise, each test will be repeated *multiple* times. The number of test runs can be configured as needed. TimedExec will then compute the *average* execution time, as well as the *fastest* and *slowest* execution time. Furthermore, the *standard deviation* of all runs will be computed. Last but not least, an optional number of "warm-up" runs will be executed *before* the first measurement.


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
