% ![](img/timedexec/banner.jpg)  
TimedExec &ndash; README
% by LoRd_MuldeR &lt;<mulder2@gmx>&gt; | <http://muldersoft.com/>

Introduction
============

**TimedExec** is a small utility for *benchmarking* command-line programs. It will *execute* the specified program with the specified command-line arguments and then *measure* the time that it takes for the execution to complete. In order to obtain *accurate* results, all measurements are implemented via *high-resolution* performance timers. And, since program execution times unavoidably are subject to certain variations (e.g. due to environmental noise), each test will be repeated *multiple* times. The number of metering passes can be configured as desired. Optionally, a number of "warm-up" passes can be performed *prior to* the first metering pass. The warm-up passes prevent caching effects from interfering with the execution times.

TimedExec will then compute the ***mean*** execution time as well as the ***median*** execution time of all metering passes. It will also record the *fastest* and *slowest* execution time that has been measured. Furthermore, TimedExec computes the *standard error* in order to determine ***confidence intervals*** from the benchmarking results^[[Konfidenzintervalle so einfach wie möglich erklärt](http://www.uni-siegen.de/phil/sozialwissenschaften/soziologie/mitarbeiter/ludwig-mayerhofer/statistik/statistik_downloads/konfidenzintervalle.pdf)]. These are the *ranges* which contain the program's “real” average execution time (expected value), *with very high probability*. All results will be saved to a log file.


Usage Instructions
==================

*TimedExec* uses a very simple command-line syntax. Just type **`TimedExec`**, followed by the program that you want to benchmark. Optionally, any number arguments can be appended; these parameters will be passed to the program.

***Note:*** Some options that influence the behavior of TimedExec can be controlled via environment variables.

```
===============================================================================
Timed Exec - Benchmarking Utility, Version 1.03
Copyright (c) 2018 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

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
  TIMED_EXEC_NO_CHECKS     - Set this to *disable* exit code checks
```

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
===============================================================================
```

Interpretation
--------------

When comparing measurement results, the ***mean*** (average) execution time may seem like the most obvious choice. However, it has to be noted that the *mean* of a data sample is highly sensitive to “outliers” and therefore can be misleading! This is especially true, when there exists a lot of variation in the data sample. Consequently, comparing the ***median*** execution times usually is the preferable choice. That is because the *median* of a data sample is much more robust against outliers.

Furthermore, it is important to keep in mind that the *mean* (or *median*) execution time computed from a limited number of metering passes only yields an ***estimate*** of the program's “real” average execution time (expected value). The “real” value can only be determined accurately from an *infitinte* number of metering passes &ndash; which is **not** possible in practice. In this situation, we can have a look at the ***confidence intervals***. These intervals contain the “real” value, *with very high probability*. The most commonly used *confidence interval* is the “95%” one (higher confidence means broader interval, and vice versa).

Simply put, as long as the confidence intervals of program A and program B *overlap* (at least partially), we **must not** conclude that either of these programs runs faster (or slower) in the average case. ***No*** conclusion can be drawn in that case!


Sources
=======

The *TimedExec* source codes are managed by [**Git**](http://git-scm.com/doc) and are available from one of the official  mirrors:

* <tt>https://github.com/lordmulder/TimedExec.git</tt> ([Browse](https://github.com/lordmulder/TimedExec))
* <tt>https://bitbucket.org/muldersoft/timedexec.git</tt> ([Browse](https://bitbucket.org/muldersoft/timedexec))
* <tt>https://gitlab.com/timedexec/timedexec.git</tt> ([Browse](https://gitlab.com/timedexec/timedexec))


License
=======

TimedExec is released under the terms of the [GNU General Public License](http://www.gnu.org/licenses/gpl-2.0.html), version 2.

```
Timed Exec - Command-Line Benchmarking Utility
Copyright (c) 2018 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.

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
