![TimedExec](img/timedexec/banner.jpg)  
by LoRd_MuldeR &lt;<mulder2@gmx>&gt; | <http://muldersoft.com/>

Introduction
============

**TimedExec** is a small utility for *benchmarking* programs. It will *execute* the specified program with the specified command-line arguments and *measure* the time that it takes for the execution to complete. Because the execution time of a program unavoidably is subject to certain variations (e.g. due to environmental noise), each measurement will be repeated *multiple* times. The number of metering passes can be configured as desired. Optionally, a number of “warm-up” passes can be performed *prior to* the metering passes. The warm-up passes prevent caching effects from interfering with the measurement.

Once all metering passes have been completed, TimedExec will compute the ***mean*** execution time as well as the ***median*** execution time of the program. It will also record the *fastest* and *slowest* execution time that has been observed. Furthermore, TimedExec computes the *standard error*, in order to determine [***confidence intervals***](http://www.uni-siegen.de/phil/sozialwissenschaften/soziologie/mitarbeiter/ludwig-mayerhofer/statistik/statistik_downloads/konfidenzintervalle.pdf) from the benchmarking results.


Usage Instructions
==================

*TimedExec* uses a very simple command-line syntax. Just type **`TimedExec`**, followed by the program that you want to benchmark. Optionally, any number arguments can be appended; these parameters will be passed to the program.

***Note:*** Some options that influence the behavior of TimedExec can be controlled via environment variables.

```
===============================================================================
Timed Exec - Benchmarking Utility, Version 1.06
Copyright (c) 2023 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License <http://www.gnu.org/>.
Note that this program is distributed with ABSOLUTELY NO WARRANTY.
===============================================================================

Usage:
  TimedExec.exe <Program.exe> [Arguments]

Influential environment variables:
  TIMED_EXEC_PASSES        - Number of execution passes (default: 5)
  TIMED_EXEC_WARMUP_PASSES - Number of warm-up passes (default: 1)
  TIMED_EXEC_LOGFILE       - Log-File Name (default: "TimedExec.log")
  TIMED_EXEC_NO_CHECKS     - Do *not* check the process exit codes
  TIMED_EXEC_NO_PRIORITY   - Do *not* adjust the process priorities
  TIMED_EXEC_NO_PERIOD     - Do *not* adjust system timer period
  TIMED_EXEC_CLOCK_TYPE    - The type of clock used for measurements
```

Clock Types
-----------

*TimedExec* supports the following clock types:

- `WALLCLOCK` &ndash; the wall-clock time, also known as *elapsed real time* (default)
- `CPU_TOTAL` &ndash; total CPU time, i.e. sum of CPU time spent in "user" *and* "kernel" modes
- `CPU_USER` &ndash; CPU time spent in "user" mode only
- `CPU_KERNEL` &ndash; CPU time spent in "kernel" mode only

Usage Example
-------------

In the following example we use *TimedExec* to benchmark the program **`ping.exe`** with the arguments **`-n 12 www.google.com`**. By default, the command will be executed *five* times, preceded by a single "warm-up" pass:

```
TimedExec.exe C:\Windows\System32\ping.exe -n 12 www.google.com
```

Results
=======

The resulting output, after all metering passes have been completed, looks like this:

```
===============================================================================
TEST COMPLETED SUCCESSFULLY AFTER 5 METERING PASSES
-------------------------------------------------------------------------------
Mean Execution Time     : 24.687 seconds
Median Execution Time   : 20.522 seconds
90% Confidence Interval : +/- 6.330 (25.643%) = [18.357, 31.018] seconds
95% Confidence Interval : +/- 7.543 (30.553%) = [17.144, 32.230] seconds
99% Confidence Interval : +/- 9.913 (40.156%) = [14.774, 34.600] seconds
Standard Deviation      : 7.697 seconds
Standard Error          : 3.848 seconds
Fastest / Slowest Pass  : 19.739 / 37.916 seconds
Active Clock Type       : WALLCLOCK (0)
===============================================================================
```

Interpretation
--------------

When comparing measurement results, the [***mean***](https://en.wikipedia.org/wiki/Arithmetic_mean) (average) execution time may seem like the most obvious choice. However, it has to be noted that the *mean* of a data sample is highly sensitive to “outliers” and therefore can be misleading! This is especially true, when there exists a lot of variation in the data sample. Consequently, comparing the [***median***](https://en.wikipedia.org/wiki/Median) of the execution times often is the better choice. That is because the *median* of a data sample is much more robust against outliers.

Furthermore, it is important to keep in mind that the *mean* (or *median*) execution time computed from a limited number of metering passes only yields an ***estimate*** of the program's “real” average execution time (expected value). The “real” value can only be determined accurately from an *infitinte* number of metering passes &ndash; which is **not** possible in practice. In this situation, we can have a look at the [***confidence intervals***](http://www.uni-siegen.de/phil/sozialwissenschaften/soziologie/mitarbeiter/ludwig-mayerhofer/statistik/statistik_downloads/konfidenzintervalle.pdf). These intervals contain the “real” value, *with very high probability*. The most commonly used *confidence interval* is the “95%” one. Higher confidence means broader interval, and vice versa.

Simply put, as long as the confidence intervals of the runtime of program “A” and the runtime of program “B” *overlap*, we **must not** conclude that either of these programs runs faster (or slower). In fact, **no** real conclusion can be drawn in that case!

Limitations
-----------

This tools measures the runtime of *processes*. Because creating a process has a certain overhead, and because the system timer has a limited precision &ndash; usually in the range of a few milliseconds, but can be worse &ndash; this tool is **not** suitable for benchmarking programs or functions with *very short* runtime! The process to be measured should run *at least* for a couple of seconds, in order to get useful benchmark results. If you need to benchmark functions with *very short* runtime, it is recommended to use [*high-precision timers*](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) directly inside your program code, rather than launching separate processes.


Sources
=======

The *TimedExec* source codes are managed by [**Git**](http://git-scm.com/doc) and are available from one of the official  mirrors:

* `https://github.com/lordmulder/TimedExec.git` ([Browse](https://github.com/lordmulder/TimedExec))
* `https://bitbucket.org/muldersoft/timedexec.git` ([Browse](https://bitbucket.org/muldersoft/timedexec))
* `https://gitlab.com/timedexec/timedexec.git` ([Browse](https://gitlab.com/timedexec/timedexec))
* `https://punkindrublic.mooo.com:3000/Muldersoft/TimedExec.git` ([Browse](https://punkindrublic.mooo.com:3000/Muldersoft/TimedExec))

License
=======

TimedExec is released under the terms of the [GNU General Public License](http://www.gnu.org/licenses/gpl-2.0.html), version 2.

```
Timed Exec - Command-Line Benchmarking Utility
Copyright (c) 2018-2023 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

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
