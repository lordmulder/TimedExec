//////////////////////////////////////////////////////////////////////////////////
// Timed Exec - Command-Line Benchmarking Utility
// Copyright (c) 2023 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
//////////////////////////////////////////////////////////////////////////////////

#include "Version.h"

#include <cstdlib>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <tchar.h>
#include <io.h>
#include <sys/stat.h>

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <ShellAPI.h>

#if _WIN32_WINNT >= 0x0603
#  include <timeapi.h>
#else
#  include <MMSystem.h>
#endif

#define DEFAULT_EXEC_LOOPS 5
#define DEFAULT_WARMUP_LOOPS 1
#define DEFAULT_LOGFILE "TimedExec.log"
#define ENABLE_ENV_VARS true

#ifdef _UNICODE
#  define tstring std::wstring
#else
#  define tstring std::string
#endif

static HANDLE g_hAbortEvent = NULL;
static volatile bool g_aborted = false;

/* clock for time measurement */
typedef enum
{
	CLOCK_WALLCLOCK,
	CLOCK_CPU_TOTAL,
	CLOCK_CPU_USER,
	CLOCK_CPU_KERNEL
}
clock_type_t;

// =============================================================================================================
// INTERNAL FUNCTIONS
// =============================================================================================================

#define _PRINT_CLOCK_TYPE(X) case CLOCK_##X: return #X;
#define _PARSE_CLOCK_TYPE(X) if (!_tcsicmp(name.c_str(), _T(#X))) { clock_type = (CLOCK_##X); return true; }

static bool parseClockType(const tstring &name, clock_type_t &clock_type)
{
	_PARSE_CLOCK_TYPE(WALLCLOCK)
	_PARSE_CLOCK_TYPE(CPU_TOTAL)
	_PARSE_CLOCK_TYPE(CPU_USER)
	_PARSE_CLOCK_TYPE(CPU_KERNEL)
	return false;
}

static const char *printClockType(const clock_type_t clock_type)
{
	switch (clock_type)
	{
		_PRINT_CLOCK_TYPE(WALLCLOCK)
		_PRINT_CLOCK_TYPE(CPU_TOTAL)
		_PRINT_CLOCK_TYPE(CPU_USER)
		_PRINT_CLOCK_TYPE(CPU_KERNEL)
	}
	return "N/A";
}

static bool setTimerResolution(UINT& period)
{
	TIMECAPS timecaps;
	if (timeGetDevCaps(&timecaps, sizeof(TIMECAPS)) == MMSYSERR_NOERROR)
	{
		if (timeBeginPeriod(timecaps.wPeriodMin) == MMSYSERR_NOERROR)
		{
			period = timecaps.wPeriodMin;
			return true;
		}
	}
	period = MAXUINT;
	return false;
}

static void resetTimerResolution(const UINT period)
{
	if (period != MAXUINT)
	{
		timeEndPeriod(period);
	}
}

static tstring trim(const std::vector<_TCHAR> &buffer)
{
	std::vector<_TCHAR>::const_iterator left;
	for (left = buffer.cbegin(); (left != buffer.cend()) && (*left <= 0x20); ++left);
	if (left != buffer.cend())
	{
		std::vector<_TCHAR>::const_reverse_iterator right;
		for (right = buffer.crbegin(); (right != buffer.crend()) && (*right <= 0x20); ++right);
		if (right != buffer.crend())
		{
			return tstring(left, right.base());
		}
	}
	return tstring();
}

static bool getEnvVariable(const _TCHAR *const name, tstring &value)
{
	std::vector<_TCHAR> buffer(MAX_PATH);
	value.clear();
	for (int i = 0; i < 3; ++i)
	{
		const DWORD result = GetEnvironmentVariable(name, buffer.data(), (DWORD)buffer.size());
		if (!result)
		{
			break; /*failed*/
		}
		else if (result > buffer.size())
		{
			buffer.resize(result);
		}
		else if (result < buffer.size())
		{
			value = trim(buffer);
			return true;
		}
	}
	return false;
}

static bool parseFlag(const tstring &value)
{
	if ((!_tcsicmp(value.c_str(), _T("yes"))) || (!_tcsicmp(value.c_str(), _T("true"))))
	{
		return true;
	}
	return (_tcstol(value.c_str(), NULL, 10) > 0L);
}

static ULONGLONG fileTimeToU64(const PFILETIME fileTime)
{
	ULARGE_INTEGER temp;
	temp.HighPart = fileTime->dwHighDateTime;
	temp.LowPart = fileTime->dwLowDateTime;
	return temp.QuadPart;
}

static ULONGLONG getTimeElapsed(const ULONGLONG timeStart, const ULONGLONG timeExit)
{
	return (timeExit > timeStart) ? (timeExit - timeStart) : 0ULL;
}

static double getProcessTime(const HANDLE hProc, const clock_type_t clock_type)
{
	FILETIME timeStart, timeExit, timeKernel, timeUser;
	ULONGLONG result = 0ULL;
	if (GetProcessTimes(hProc, &timeStart, &timeExit, &timeKernel, &timeUser))
	{
		switch (clock_type)
		{
		case CLOCK_WALLCLOCK:
			result = getTimeElapsed(fileTimeToU64(&timeStart), fileTimeToU64(&timeExit));
			break;
		case CLOCK_CPU_USER:
			result = fileTimeToU64(&timeUser);
			break;
		case CLOCK_CPU_KERNEL:
			result = fileTimeToU64(&timeKernel);
			break;
		case CLOCK_CPU_TOTAL:
			result = fileTimeToU64(&timeKernel) + fileTimeToU64(&timeUser);
			break;
		}
	}
	return static_cast<double>(result) / 10000000.0;
}

static long long getCurrentFileSize(FILE *const filePtr)
{
	struct _stati64 stats;
	if (_fstati64(_fileno(filePtr), &stats) == 0)
	{
		return stats.st_size;
	}
	return -1;
}

static bool checkBinary(const tstring &filePath)
{
	DWORD binaryType;
	return GetBinaryType(filePath.c_str(), &binaryType) ? true : false;
}

static tstring getFullPath(const _TCHAR *const fileName)
{
	std::vector<TCHAR> buffer(MAX_PATH);
	for (int i = 0; i < 3; ++i)
	{
		const DWORD result = GetFullPathName(fileName, (DWORD)buffer.size(), buffer.data(), NULL);
		if (!result)
		{
			break; /*failed*/
		}
		else if (result > buffer.size())
		{
			buffer.resize(result);
		}
		else if (result < buffer.size())
		{
			return tstring(buffer.begin(), buffer.end());
		}
	}
	return tstring(fileName);
}

static tstring getFileNameOnly(const tstring &filePath)
{
	for (tstring::const_reverse_iterator iter = filePath.crbegin(); iter != filePath.crend(); ++iter)
	{
		if ((*iter == _T('/')) || (*iter == _T('\\')))
		{
			tstring::const_iterator offset = iter.base();
			if (offset != filePath.cend())
			{
				return tstring(offset, filePath.cend());
			}
			return tstring();
		}
	}
	return filePath;
}

static double computeMedian(std::vector<double> &data)
{
	if (data.empty())
	{
		return 0.0;
	}

	std::sort(data.begin(), data.end());

	const size_t size = data.size();
	const size_t center = size / 2U;

	return (!(size & 1U))
		? ((data[center - 1U] + data[center]) / 2.0)
		: data[center];
}

static void appendStr(tstring &commandLine, const tstring &token)
{
	for (tstring::const_iterator iter = token.cbegin(); iter != token.cend(); ++iter)
	{
		if (*iter)
		{
			if (*iter == _T('"'))
			{
				commandLine += _T('\\');
			}
			commandLine += *iter;
		}
		else
		{
			break;
		}
	}
}

static int initializeCommandLine(tstring &commandLine, tstring &programFile)
{
	commandLine.clear();
	programFile.clear();

	int nArgs = 0;
	TCHAR **szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

	if ((szArglist == NULL) || (nArgs < 2))
	{
		return 0;
	}

	for(int i = 1; i < nArgs; i++)
	{
		tstring token;
		if (i > 1)
		{
			token = tstring(szArglist[i]);
			commandLine += _T(' ');
		}
		else
		{
			token = getFullPath(szArglist[i]);
			programFile = token;
		}
		if (token.find(_T(' ')) == tstring::npos)
		{
			appendStr(commandLine, token);
		}
		else
		{
			commandLine += _T('"');
			appendStr(commandLine, token);
			commandLine += _T('"');
		}
	}

	LocalFree(szArglist);
	return nArgs;
}

static bool createProcess(const tstring &commandLine, HANDLE &hThrd, HANDLE &hProc, const bool highPriority = false)
{
	STARTUPINFO startInfo;
	SecureZeroMemory(&startInfo, sizeof(STARTUPINFO));
	PROCESS_INFORMATION processInfo;
	SecureZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

	if (!CreateProcess(NULL, (LPTSTR)commandLine.c_str(), NULL, NULL, false, highPriority ? HIGH_PRIORITY_CLASS : 0U, NULL, NULL, &startInfo, &processInfo))
	{
		return false;
	}

	hThrd = processInfo.hThread;
	hProc = processInfo.hProcess;

	return true;
}

static bool waitForProcess(const HANDLE &hProc)
{
	HANDLE waitHandles[2] = {hProc, g_hAbortEvent};
	const DWORD ret = WaitForMultipleObjects(2, &waitHandles[0], FALSE, INFINITE);
	if ((ret != WAIT_OBJECT_0) && (ret != WAIT_OBJECT_0 + 1))
	{
		return false;
	}
	if (ret > WAIT_OBJECT_0)
	{
		g_aborted = true;
	}
	return true;
}

static int getProcessExitCode(const HANDLE &hProc)
{
	DWORD exitCode;
	if (GetExitCodeProcess(hProc, &exitCode))
	{
		return *reinterpret_cast<int*>(&exitCode);
	}
	return -1;
}

static void abortedHandlerRoutine(const HANDLE &hProc)
{
	TerminateProcess(hProc, UINT(-1));
	WaitForSingleObject(hProc, INFINITE);
	std::cerr << std::endl;
	std::cerr << "\n===============================================================================" << std::endl;
	std::cerr << "ABORTED BY USER !!!" << std::endl;
	std::cerr << "===============================================================================\n" << std::endl;
}

static void invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	std::cerr << std::endl;
	std::cerr << "\n===============================================================================" << std::endl;
	std::cerr << "GURU MEDITATION: Invalid Parameter Handler Invoked!" << std::endl;
	std::cerr << "===============================================================================\n" << std::endl;
	_exit(-1);
}

static BOOL WINAPI ctrlHandlerRoutine(DWORD dwCtrlType)
{
	g_aborted = true;
	SetEvent(g_hAbortEvent);
	return TRUE;
}

static LONG WINAPI crashHandlerRoutine(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	static const char *const message = "\n\nGURU MEDITATION: UNHANDELED SYSTEM EXCEPTION !!!\n\n";
	DWORD bytesWritten;
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, lstrlenA(message), &bytesWritten, NULL);
	TerminateProcess(GetCurrentProcess(), UINT(-1));
	return EXCEPTION_EXECUTE_HANDLER;
}

// =============================================================================================================
// MAIN FUNCTION
// =============================================================================================================

#define PROCESS_FAILED() do { CloseHandle(hThrd); CloseHandle(hProc); goto cleanup; } while (0)

static int timedExecMain(int argc, _TCHAR* argv[])
{
	int exitCode = EXIT_FAILURE;
	std::ios initFmt(NULL);
	initFmt.copyfmt(std::cerr);

	std::cerr << "\n===============================================================================" << std::endl;
	std::cerr << "Timed Exec - Benchmarking Utility, Version " << VERSION_MAJOR << '.' << std::setfill('0') << std::setw(2) << (10 * VERSION_MINOR_HI) + VERSION_MINOR_LO << " [" __DATE__ "]" << std::endl;
	std::cerr << "Copyright (c) 2023 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n" << std::endl;
	std::cerr << "This program is free software: you can redistribute it and/or modify" << std::endl;
	std::cerr << "it under the terms of the GNU General Public License <http://www.gnu.org/>." << std::endl;
	std::cerr << "Note that this program is distributed with ABSOLUTELY NO WARRANTY." << std::endl;
	std::cerr << "===============================================================================\n" << std::endl;

	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Check Command-Line                                         */
	/* ---------------------------------------------------------- */

	tstring commandLine, programFile;
	if (initializeCommandLine(commandLine, programFile) < 2)
	{
		std::cerr << "Usage:" << std::endl;
		std::cerr << "  TimedExec.exe <Program.exe> [Arguments]\n" << std::endl;
		std::cerr << "Influential environment variables:" << std::endl;
		std::cerr << "  TIMED_EXEC_PASSES        - Number of execution passes (default: " << DEFAULT_EXEC_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_EXEC_WARMUP_PASSES - Number of warm-up passes (default: " << DEFAULT_WARMUP_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_EXEC_LOGFILE       - Log-File Name (default: \"" << DEFAULT_LOGFILE << "\")" << std::endl;
		std::cerr << "  TIMED_EXEC_NO_CHECKS     - Do *not* check the process exit codes" << std::endl;
		std::cerr << "  TIMED_EXEC_NO_PRIORITY   - Do *not* adjust the process priorities" << std::endl;
		std::cerr << "  TIMED_EXEC_NO_PERIOD     - Do *not* adjust system timer period" << std::endl;
		std::cerr << "  TIMED_EXEC_CLOCK_TYPE    - The type of clock used for measurements\n" << std::endl;
		return EXIT_FAILURE;
	}

	if (_taccess(programFile.c_str(), 0) != 0)
	{
		_ftprintf(stderr, _T("Specified program file could not be found or access denied:\n%s\n\n"), programFile.c_str());
		return EXIT_FAILURE;
	}

	if (!checkBinary(programFile))
	{
		_ftprintf(stderr, _T("Specified file does not look like a valid Win32 executable:\n%s\n\n"), programFile.c_str());
		return EXIT_FAILURE;
	}

	/* ---------------------------------------------------------- */
	/* Check Environment Variables                                */
	/* ---------------------------------------------------------- */

	tstring logFile(getFullPath(_T(DEFAULT_LOGFILE)));
	clock_type_t clock_type = CLOCK_WALLCLOCK;
	int maxPasses = DEFAULT_EXEC_LOOPS, maxWarmUpPasses = DEFAULT_WARMUP_LOOPS;
	bool checkExitCodes = true, adjustPriority = true, adjustPeriod = true;

	if (ENABLE_ENV_VARS)
	{
		tstring temp;
		if (getEnvVariable(_T("TIMED_EXEC_PASSES"), temp))
		{
			maxPasses = std::min(SHRT_MAX, std::max(3, _tstoi(temp.c_str())));
		}
		if (getEnvVariable(_T("TIMED_EXEC_WARMUP_PASSES"), temp))
		{
			maxWarmUpPasses = std::min(SHRT_MAX, std::max(0, _tstoi(temp.c_str())));
		}
		if (getEnvVariable(_T("TIMED_EXEC_LOGFILE"), temp))
		{
			logFile = getFullPath(temp.c_str());
		}
		if (getEnvVariable(_T("TIMED_EXEC_NO_CHECKS"), temp))
		{
			checkExitCodes = (!parseFlag(temp));
		}
		if (getEnvVariable(_T("TIMED_EXEC_NO_PRIORITY"), temp))
		{
			adjustPriority = (!parseFlag(temp));
		}
		if (getEnvVariable(_T("TIMED_EXEC_NO_PERIOD"), temp))
		{
			adjustPeriod = (!parseFlag(temp));
		}
		if (getEnvVariable(_T("TIMED_EXEC_CLOCK_TYPE"), temp))
		{
			if (!parseClockType(temp, clock_type))
			{
				_ftprintf(stderr, _T("WARNING: Specified clock type \"%s\" is unsupported. Using default clock type!\n\n"), temp.c_str());
			}
		}
	}

	/* ---------------------------------------------------------- */
	/* Initialization                                             */
	/* ---------------------------------------------------------- */

	_ftprintf(stderr, _T("Command-line:\n%s\n\n"), commandLine.c_str());
	_ftprintf(stderr, _T("Log File:\n%s\n\n"), logFile.c_str());
	std::cerr << "Warm-Up / Metering passes: " << maxWarmUpPasses << "x / " << maxPasses << 'x' << std::endl;

	if (adjustPriority)
	{
		if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
		{
			std::cerr << "\nWARNING: Failed to adjust process priroity class!" << std::endl;
		}
	}

	UINT timerPeriod = MAXUINT;
	if (adjustPeriod)
	{
		if (!setTimerResolution(timerPeriod))
		{
			std::cerr << "\nWARNING: Failed to adjust timer period!" << std::endl;

		}
	}

	std::vector<double> stats_samples(maxPasses, 0.0);

	double stats_mean     = 0.0;
	double stats_variance = 0.0;
	double stats_fastest  = DBL_MAX;
	double stats_slowest  = 0.0;

	/* ---------------------------------------------------------- */
	/* Run Warm-Up Passes                                         */
	/* ---------------------------------------------------------- */

	for (int pass = 0; pass < maxWarmUpPasses; pass++)
	{
		std::cerr << "\n===============================================================================" << std::endl;
		if (maxWarmUpPasses > 1) std::cerr << "WARM-UP PASS " << (pass + 1) << " OF " << maxWarmUpPasses << std::endl; else std::cerr << "WARM-UP PASS" << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;

		HANDLE hThrd, hProc;

		if (!createProcess(commandLine, hThrd, hProc, adjustPriority))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to create process!\n" << std::endl;
			goto cleanup;
		}

		if (!waitForProcess(hProc))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to wait for process termination!\n" << std::endl;
			PROCESS_FAILED();
		}

		if (g_aborted)
		{
			abortedHandlerRoutine(hProc);
			PROCESS_FAILED();
		}

		const int exitCode = getProcessExitCode(hProc);
		if (checkExitCodes && (exitCode != 0))
		{
			std::cerr << "\n\nPROGRAM ERROR: Abnormal program termination detected! (Exit Code: " << exitCode << ")\n" << std::endl;
			PROCESS_FAILED();
		}
		else
		{
			std::cerr << "\n>> Process terminated with exit code " << exitCode << '.' << std::endl;
		}

		CloseHandle(hThrd);
		CloseHandle(hProc);
	}

	/* ---------------------------------------------------------- */
	/* Run Execution Passes                                       */
	/* ---------------------------------------------------------- */

	for(int pass = 0; pass < maxPasses; pass++)
	{
		std::cerr << "\n===============================================================================" << std::endl;
		if (maxPasses > 1) std::cerr << "METERING PASS " << (pass + 1) << " OF " << maxPasses << std::endl; else std::cerr << "METERING PASS" << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;

		HANDLE hThrd, hProc;

		if (!createProcess(commandLine, hThrd, hProc, adjustPriority))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to create process!\n" << std::endl;
			goto cleanup;
		}
		
		if (!waitForProcess(hProc))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to wait for process termination!\n" << std::endl;
			PROCESS_FAILED();
		}

		if (g_aborted)
		{
			abortedHandlerRoutine(hProc);
			PROCESS_FAILED();
		}

		const int exitCode = getProcessExitCode(hProc);
		if (checkExitCodes && (exitCode != 0))
		{
			std::cerr << "\n\nPROGRAM ERROR: Abnormal program termination detected! (Exit Code: " << exitCode << ")\n" << std::endl;
			PROCESS_FAILED();
		}
		else
		{
			std::cerr << "\n>> Process terminated with exit code " << exitCode << '.' << std::endl;
		}

		const double execTime = getProcessTime(hProc, clock_type);

		std::cerr << std::setprecision(3) << std::fixed;
		std::cerr << ">> Execution took " << execTime << " seconds." << std::endl;
		std::cerr.copyfmt(initFmt);

		CloseHandle(hThrd);
		CloseHandle(hProc);

		// Store this sample
		stats_samples[pass] = execTime;

		// Update slowest/fastest
		if (execTime > stats_slowest) stats_slowest = execTime;
		if (execTime < stats_fastest) stats_fastest = execTime;

		// Iterative "online" computation of the mean and the variance
		// See http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm for details!
		const double delta = execTime - stats_mean;
		stats_mean += delta / ((double)(pass + 1));
		stats_variance += delta * (execTime - stats_mean);
	}

	stats_variance /= ((double)(maxPasses - 1));

	/* ---------------------------------------------------------- */
	/* Print Results                                              */
	/* ---------------------------------------------------------- */

	// Compute the "standard error" and the "confidence" intervalls for our measurement
	// See http://www.uni-siegen.de/phil/sozialwissenschaften/soziologie/mitarbeiter/ludwig-mayerhofer/statistik/statistik_downloads/konfidenzintervalle.pdf for details!
	const double standardDeviation = sqrt(stats_variance);
	const double standardError = standardDeviation / sqrt((double)(maxPasses - 1));
	const double confidenceInterval_90 = 1.645 * standardError;
	const double confidenceInterval_95 = 1.960 * standardError;
	const double confidenceInterval_99 = 2.576 * standardError;

	//Compute median
	const double medianTime = computeMedian(stats_samples);

	//Print results
	std::cerr << std::setprecision(3) << std::fixed;
	std::cerr << "\n===============================================================================" << std::endl;
	std::cerr << "TEST COMPLETED SUCCESSFULLY AFTER " << maxPasses << " METERING PASSES" << std::endl;
	std::cerr << "-------------------------------------------------------------------------------" << std::endl;
	std::cerr << "Mean Execution Time     : " << stats_mean << " seconds" << std::endl;
	std::cerr << "Median Execution Time   : " << medianTime << " seconds" << std::endl;
	std::cerr << "90% Confidence Interval : +/- " << confidenceInterval_90 << " (" << 100.0 * (confidenceInterval_90 / stats_mean) << "%) = [" << (stats_mean - confidenceInterval_90) << ", " << (stats_mean + confidenceInterval_90) << "] seconds" << std::endl;
	std::cerr << "95% Confidence Interval : +/- " << confidenceInterval_95 << " (" << 100.0 * (confidenceInterval_95 / stats_mean) << "%) = [" << (stats_mean - confidenceInterval_95) << ", " << (stats_mean + confidenceInterval_95) << "] seconds" << std::endl;
	std::cerr << "99% Confidence Interval : +/- " << confidenceInterval_99 << " (" << 100.0 * (confidenceInterval_99 / stats_mean) << "%) = [" << (stats_mean - confidenceInterval_99) << ", " << (stats_mean + confidenceInterval_99) << "] seconds" << std::endl;
	std::cerr << "Standard Deviation      : " << standardDeviation << " seconds" << std::endl;
	std::cerr << "Standard Error          : " << standardError << " seconds" << std::endl;
	std::cerr << "Fastest / Slowest Pass  : " << stats_fastest << " / " << stats_slowest << " seconds" << std::endl;
	std::cerr << "Active Clock Type       : " << printClockType(clock_type) << " (" << clock_type << ')' << std::endl;
	std::cerr << "===============================================================================\n" << std::endl;
	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Write Log-File                                             */
	/* ---------------------------------------------------------- */

	FILE *fLog = NULL;
	if (_tfopen_s(&fLog, logFile.c_str(), _T("a+")) == 0)
	{
		if (getCurrentFileSize(fLog) == 0)
		{
			_ftprintf_s(fLog, _T("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n"), _T("Program"), _T("Passes"), _T("Mean Time"), _T("Median Time"), _T("90% Confidence Interval"), _T("95% Confidence Interval"), _T("99% Confidence Interval"), _T("Fastest Pass"), _T("Slowest Pass"), _T("Standard Deviation"), _T("Standard Error"), _T("Command Line"));
		}
		_ftprintf_s(fLog, _T("%s\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%s\n"), getFileNameOnly(programFile).c_str(), maxPasses, stats_mean, medianTime, confidenceInterval_90, confidenceInterval_95, confidenceInterval_99, stats_fastest, stats_slowest, standardDeviation, standardError, commandLine.c_str());
		if (ferror(fLog) == 0)
		{
			_ftprintf(stderr, _T("Log file updated successfully.\n\n"));
		}
		else
		{
			_ftprintf(stderr, _T("Failed to append data to the specified log file:\n%s\n\n"), logFile.c_str());
		}
		fclose(fLog); fLog = NULL;
	}
	else
	{
		_ftprintf(stderr, _T("Failed to open the specified log file for writing:\n%s\n\n"), logFile.c_str());
	}

	/* ---------------------------------------------------------- */
	/* Goodbye!                                                   */
	/* ---------------------------------------------------------- */

	exitCode = EXIT_SUCCESS;

cleanup:
	resetTimerResolution(timerPeriod);
	return exitCode;
}

// =============================================================================================================
// APPLICATION ENTRY POINT
// =============================================================================================================

static int mainEx(int argc, _TCHAR* argv[])
{
	int ret = EXIT_FAILURE;

	try
	{
		ret = timedExecMain(argc, argv);
	}
	catch(std::exception &e)
	{
		std::cerr << std::endl;
		std::cerr << "\n===============================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (" << e.what() << ')' << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;
		_exit(-1);
	}
	catch(std::exception *e)
	{
		std::cerr << std::endl;
		std::cerr << "\n===============================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (" << e->what() << ')' << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;
		_exit(-1);
	}	catch(...)
	{
		std::cerr << std::endl;
		std::cerr << "\n===============================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (Unknown Exception Type)" << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;
		_exit(-1);
	}

	return ret;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int ret = EXIT_FAILURE;

#ifndef _DEBUG
	SetUnhandledExceptionFilter(crashHandlerRoutine);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_invalid_parameter_handler(invalidParameterHandler);
#endif // _DEBUG

	if (!(g_hAbortEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		std::cerr << "\n\nSYSTEM ERROR: Event object could not be created!\n" << std::endl;
		return EXIT_FAILURE;
	}
	
	SetConsoleCtrlHandler(ctrlHandlerRoutine, TRUE);

#ifndef _DEBUG
	__try
	{
		ret = mainEx(argc, argv);
	}
	__except(crashHandlerRoutine(GetExceptionInformation()))
	{
		for(;;) _exit(-1);
	}
#else
	ret = timedExecMain(argc, argv);
#endif // _DEBUG

	return ret;
}
