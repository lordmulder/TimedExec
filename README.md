TimedExec
=========

**TimedExec** is a small utility for *benchmarking* command-line programs. It will *execute* the specified program with the specified command-line arguments and then *measure* the time that it takes for the program execution to complete. In order to obtain *accurate* results, all measurements are implemented via *high-resolution* performance timers. And, since program execution times unavoidably are subject to variations (due to environmental noise), each test will be repeated *multiple* times. The number of metering passes can be configured as desired. TimedExec will then compute the *mean* execution time of all passes. It will also record the *fastest* and *slowest* execution time. Furthermore, TimedExec computes the *standard error* as well as the *confidence interval* from the benchmarking results. This is the *range* which contains the program's actual (mean) execution time, *with very high probability*. Last but not least, an optional number of "warm-up" passes can be performed *prior to* the first metering pass. The warm-up passes prevent caching effects from interfering with the execution times. Note that all benchmarking results will be saved to a log file.


Usage Instructions
------------------

### Command-line Syntax ###

TimedExec uses a very simple command-line syntax. Just type "TimedExec", followed by the program that you want to benchmark, followed by the desired arguments.

*Note:* Some parameters that influence the behaviour of TimedExec can be set via environment variables.

```
===============================================================================
Timed Exec - Benchmarking Utility, Version 1.03 [Nov  7 2014]
Copyright (c) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License <http://www.gnu.org/>.
Note that this program is distributed with ABSOLUTELY NO WARRANTY.
===============================================================================

Usage:
  TimedExec.exe <Program.exe> [Arguments]

Influential environment variables:
  TIMED_EXEC_PASSES        - Number of execution passes (default: 10)
  TIMED_EXEC_WARMUP_PASSES - Number of warm-up passes (default: 3)
  TIMED_EXEC_LOGFILE       - Log-File Name (default: "TimedExec.log")
  TIMED_EXEC_NO_CHECKS     - Set this to *disable* exit code checks
```

### Usage Example ###

In the following example we use *TimedExec* to benchmark the program **ping.exe** with the arguments **-n 12 www.google.com**:

```
TimedExec.exe C:\Windows\System32\ping.exe -n 12 www.google.com
```

The resulting output, after all ten passes have been completed, may look like this:
```
===============================================================================
TEST COMPLETED SUCCESSFULLY AFTER 10 METERING PASSES
-------------------------------------------------------------------------------
Mean Execution Time     : 13.838 seconds
90% Confidence Interval : +/- 1.001 ( 7.235%) = [12.836, 14.839] seconds
95% Confidence Interval : +/- 1.193 ( 8.621%) = [12.645, 15.031] seconds
99% Confidence Interval : +/- 1.568 (11.330%) = [12.270, 15.405] seconds
Standard Deviation      : 1.826 seconds
Standard Error          : 0.609 seconds
Fastest / Slowest Pass  : 11.118 / 15.075 seconds
===============================================================================
```


License
-------

TimedExec is released under the terms of the [GNU General Public License](http://www.gnu.org/licenses/gpl-2.0.html), version 2.

```
Timed Exec - Command-Line Benchmarking Utility
Copyright (c) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

http://www.gnu.org/licenses/gpl-2.0.txt
```

<br>

**e.o.f.**
