//////////////////////////////////////////////////////////////////////////////////
// Timed Exec - Command-Line Benchmarking Utility
// Copyright (c) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.
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

#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <tchar.h>
#include <sys/stat.h>

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <ShellAPI.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 3

#define EXEC_LOOPS   10
#define WARMUP_LOOPS 3

#define LOG_FILE _T("TimedExec.log")

static HANDLE g_hAbortEvent = NULL;
static volatile bool g_aborted = false;

// =============================================================================================================
// INTERNAL FUNCTIONS
// =============================================================================================================

static bool getEnvVariable(const _TCHAR *const name, _TCHAR *const buffer, size_t buffSize)
{
	const DWORD ret = GetEnvironmentVariable(name, buffer, buffSize);
	return ((ret > 0) && (ret < buffSize));
}

static LONGLONG getCurrentTime(void)
{
	LARGE_INTEGER timeValue;
	if(!QueryPerformanceCounter(&timeValue))
	{
		std::cerr << "\nTimedExec: Failed to query performance counter!\n" << std::endl;
		exit(EXIT_FAILURE);
	}
	return timeValue.QuadPart;
}

static LONGLONG getTimerFrequency(void)
{
	LARGE_INTEGER timeValue;
	if(!QueryPerformanceFrequency(&timeValue))
	{
		std::cerr << "\nTimedExec: Failed to query performance counter!\n" << std::endl;
		exit(EXIT_FAILURE);
	}
	return timeValue.QuadPart;
}

static bool createProcess(_TCHAR *const commandLine, HANDLE &hThrd, HANDLE &hProc, const bool suspended = false)
{
	STARTUPINFO startInfo;
	SecureZeroMemory(&startInfo, sizeof(STARTUPINFO));
	PROCESS_INFORMATION processInfo;
	SecureZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

	if(!CreateProcess(NULL, commandLine, NULL, NULL, false, HIGH_PRIORITY_CLASS | (suspended ? CREATE_SUSPENDED : 0), NULL, NULL, &startInfo, &processInfo))
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
	if((ret != WAIT_OBJECT_0) && (ret != WAIT_OBJECT_0 + 1))
	{
		return false;
	}
	if(ret > WAIT_OBJECT_0)
	{
		g_aborted = true;
	}
	return true;
}

static void abortedHandlerRoutine(const HANDLE &hProc)
{
	TerminateProcess(hProc, UINT(-1));
	WaitForSingleObject(hProc, INFINITE);
	std::cerr << std::endl;
	std::cerr << "\n===========================================================================" << std::endl;
	std::cerr << "ABORTED BY USER !!!" << std::endl;
	std::cerr << "===========================================================================\n" << std::endl;
}

static void invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	std::cerr << std::endl;
	std::cerr << "\n===========================================================================" << std::endl;
	std::cerr << "GURU MEDITATION: Invalid Parameter Handler Invoked!" << std::endl;
	std::cerr << "===========================================================================\n" << std::endl;
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
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, strlen(message), &bytesWritten, NULL);
	TerminateProcess(GetCurrentProcess(), UINT(-1));
	return EXCEPTION_EXECUTE_HANDLER;
}

// =============================================================================================================
// MAIN FUNCTION
// =============================================================================================================

static int timedExecMain(int argc, _TCHAR* argv[])
{
	std::ios initFmt(NULL);
	initFmt.copyfmt(std::cerr);

	std::cerr << "\n===========================================================================" << std::endl;
	std::cerr << "Timed Exec - Benchmarking Utility, Version " << VERSION_MAJOR << '.' << std::setfill('0') << std::setw(2) << VERSION_MINOR << " [" __DATE__ "]" << std::endl;
	std::cerr << "Copyright (c) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n" << std::endl;
	std::cerr << "This program is free software: you can redistribute it and/or modify" << std::endl;
	std::cerr << "it under the terms of the GNU General Public License <http://www.gnu.org/>." << std::endl;
	std::cerr << "Note that this program is distributed with ABSOLUTELY NO WARRANTY." << std::endl;
	std::cerr << "===========================================================================\n" << std::endl;

	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Check Command-Line                                         */
	/* ---------------------------------------------------------- */

	TCHAR *fullCommandLine = GetCommandLine();
	
	int nArgs = 0;
	TCHAR **szArglist = CommandLineToArgvW(fullCommandLine, &nArgs);

	if(szArglist == NULL)
	{
		std::cerr << std::endl << "Internal error: Initialization failed!" << std::endl;
		return EXIT_FAILURE;
	}

	if(nArgs < 2)
	{
		std::cerr << "Usage:" << std::endl;
		std::cerr << "  TimedExec.exe <Program.exe> [Arguments]\n" << std::endl;
		std::cerr << "Influential environment variables:" << std::endl;
		std::cerr << "  TIMED_EXEC_LOGFILE  - Log File (default: \"" << LOG_FILE << "\")" << std::endl;
		std::cerr << "  TIMED_EXEC_PASSES   - Number of execution passes (default: " << EXEC_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_WARMUP_PASSES - Number of warm-up passes (default: " << WARMUP_LOOPS << ")\n" << std::endl;
		return EXIT_FAILURE;
	}

	int len = std::max(_tcslen(fullCommandLine) + 1U, 4096U);
	TCHAR *myCommandLine = (TCHAR*) _alloca(sizeof(TCHAR) * len);
	myCommandLine[0] = _T('\0');

	for(int i = 1; i < nArgs; i++)
	{
		if(i > 1)
		{
			_tcsncat_s(myCommandLine, len, _T(" "), _TRUNCATE);
		}
		if(_tcschr(szArglist[i], _T(' ')) == NULL)
		{
			_tcsncat_s(myCommandLine, len, szArglist[i], _TRUNCATE);
		}
		else
		{
			_tcsncat_s(myCommandLine, len, _T("\""), _TRUNCATE);
			_tcsncat_s(myCommandLine, len, szArglist[i], _TRUNCATE);
			_tcsncat_s(myCommandLine, len, _T("\""), _TRUNCATE);
		}
	}
		
	/* ---------------------------------------------------------- */
	/* Check Environment Variables                                */
	/* ---------------------------------------------------------- */

	TCHAR temp[MAX_PATH], logFile[MAX_PATH];
	int maxLoops = EXEC_LOOPS, warmupLoops = WARMUP_LOOPS;
	_tcsncpy_s(logFile, MAX_PATH, LOG_FILE, _TRUNCATE);
	
	if(getEnvVariable(_T("TIMED_EXEC_PASSES"), temp, MAX_PATH))
	{
		maxLoops = std::max(3, _tstoi(temp));
	}
	if(getEnvVariable(_T("TIMED_WARMUP_PASSES"), temp, MAX_PATH))
	{
		warmupLoops = std::max(0, _tstoi(temp));
	}
	if(getEnvVariable(_T("TIMED_EXEC_LOGFILE"), temp, MAX_PATH))
	{
		_tcsncpy_s(logFile, MAX_PATH, temp, _TRUNCATE);
	}

	/* ---------------------------------------------------------- */
	/* Initialization                                             */
	/* ---------------------------------------------------------- */

	_ftprintf(stderr, _T("Command-line:\n%s\n"), myCommandLine);

	const LONGLONG timerFrequency = getTimerFrequency();
	if(!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
	{
		if(!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
		{
			std::cerr << "\nWARNING: Failed to set process priroity class!" << std::endl;
		}
	}

	double slowestResult = 0.0;
	double fastestResult = DBL_MAX;

	double meanResult = 0.0;
	double variance = 0.0;

	/* ---------------------------------------------------------- */
	/* Run Warm-Up Passes                                         */
	/* ---------------------------------------------------------- */

	for(int loop = 0; loop < warmupLoops; loop++)
	{
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "WARM-UP PASS " << (loop + 1) << " OF " << warmupLoops << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;

		HANDLE hThrd, hProc;

		if(!createProcess(myCommandLine, hThrd, hProc))
		{
			std::cerr << "\nTimedExec: Failed to create process!" << std::endl;
			return EXIT_FAILURE;
		}

		if(!waitForProcess(hProc))
		{
			std::cerr << "\nTimedExec: Failed to wait for process termination!" << std::endl;
			return EXIT_FAILURE;
		}

		if(g_aborted)
		{
			abortedHandlerRoutine(hProc);
			return EXIT_FAILURE;
		}

		CloseHandle(hThrd);
		CloseHandle(hProc);
	}

	/* ---------------------------------------------------------- */
	/* Run Execution Passes                                       */
	/* ---------------------------------------------------------- */

	for(int loop = 0; loop < maxLoops; loop++)
	{
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "EXECUTION PASS " << (loop + 1) << " OF " << maxLoops << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;

		HANDLE hThrd, hProc;

		if(!createProcess(myCommandLine, hThrd, hProc, true))
		{
			std::cerr << "\nTimedExec: Failed to create process!" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeStart = getCurrentTime();
		
		if(ResumeThread(hThrd) == ((DWORD) -1))
		{
			std::cerr << "\nTimedExec: Failed to resume thread!" << std::endl;
			return EXIT_FAILURE;
		}
		
		if(!waitForProcess(hProc))
		{
			std::cerr << "\nTimedExec: Failed to wait for process termination!" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeFinish = getCurrentTime();
		const double execTime = static_cast<double>(timeFinish - timeStart) / static_cast<double>(timerFrequency);

		if(g_aborted)
		{
			abortedHandlerRoutine(hProc);
			return EXIT_FAILURE;
		}

		CloseHandle(hThrd);
		CloseHandle(hProc);

		std::cerr << std::setprecision(3) << std::fixed;
		std::cerr << "\n--> Execution took " << execTime << " seconds." << std::endl;
		std::cerr.copyfmt(initFmt);

		if(execTime > slowestResult) slowestResult = execTime;
		if(execTime < fastestResult) fastestResult = execTime;

		const double delta = execTime - meanResult;
		meanResult += delta / double(loop + 1);
		variance += delta * (execTime - meanResult);
	}

	variance /= double(maxLoops - 1);

	/* ---------------------------------------------------------- */
	/* Print Results                                              */
	/* ---------------------------------------------------------- */

	const double standardDeviation = sqrt(variance);
	const double standardError = standardDeviation / sqrt(double(maxLoops - 1));

	const double confidenceInterval_90 = 1.645 * standardError;
	const double confidenceInterval_95 = 1.960 * standardError;
	const double confidenceInterval_99 = 2.576 * standardError;

	std::cerr << std::setprecision(3) << std::fixed;
	std::cerr << "\n===========================================================================" << std::endl;
	std::cerr << "TEST COMPLETED SUCCESSFULLY AFTER " << maxLoops << " EXECUTION PASSES" << std::endl;
	std::cerr << "---------------------------------------------------------------------------" << std::endl;
	std::cerr << "Mean Execution Time     : " << meanResult << " seconds" << std::endl;
	std::cerr << "90% Confidence Interval : +/- " << confidenceInterval_90 << " = [" << (meanResult - confidenceInterval_90) << ", " << (meanResult + confidenceInterval_90) << "] seconds" << std::endl;
	std::cerr << "95% Confidence Interval : +/- " << confidenceInterval_95 << " = [" << (meanResult - confidenceInterval_95) << ", " << (meanResult + confidenceInterval_95) << "] seconds" << std::endl;
	std::cerr << "99% Confidence Interval : +/- " << confidenceInterval_99 << " = [" << (meanResult - confidenceInterval_99) << ", " << (meanResult + confidenceInterval_99) << "] seconds" << std::endl;
	std::cerr << "Standard Deviation      : " << standardDeviation << " seconds" << std::endl;
	std::cerr << "Standard Error          : " << standardError << " seconds" << std::endl;
	std::cerr << "Fastest / Slowest Pass  : " << fastestResult << " / " << slowestResult << " seconds" << std::endl;
	std::cerr << "===========================================================================\n" << std::endl;
	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Write Log-File                                             */
	/* ---------------------------------------------------------- */

	FILE *fLog = NULL;
	if(_tfopen_s(&fLog, logFile, _T("a+")) == 0)
	{
		struct _stati64 stats;
		if(_fstati64(_fileno(fLog), &stats) == 0)
		{
			if(stats.st_size == 0)
			{
				_ftprintf_s(fLog, _T("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n"), _T("Program"), _T("Passes"), _T("Mean Time"), _T("90% Confidence Interval"), _T("95% Confidence Interval"), _T("99% Confidence Interval"), _T("Fastest Pass"), _T("Slowest Pass"), _T("Standard Deviation"), _T("Standard Error"), _T("Command"));
			}
		}
		_tcsncpy_s(temp, MAX_PATH, szArglist[1], _TRUNCATE);
		TCHAR *ctx, *exeName = _tcstok_s(temp, _T(":/\\"), &ctx);
		while(TCHAR *tok = _tcstok_s(NULL, _T(":/\\"), &ctx)) exeName = tok;
		_ftprintf_s(fLog, _T("%s\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%s\n"), exeName, maxLoops, meanResult, confidenceInterval_90, confidenceInterval_95, confidenceInterval_99, fastestResult, slowestResult, standardDeviation, standardError, myCommandLine);
		fclose(fLog); fLog = NULL;
	}
	else
	{
		std::cerr << "Error: Failed to append results to log file!\n" << std::endl;
	}

	/* ---------------------------------------------------------- */
	/* Final Clean-up                                             */
	/* ---------------------------------------------------------- */

	LocalFree(szArglist);
	return EXIT_SUCCESS;
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
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (" << e.what() << ')' << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;
		_exit(-1);
	}
	catch(std::exception *e)
	{
		std::cerr << std::endl;
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (" << e->what() << ')' << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;
		_exit(-1);
	}	catch(...)
	{
		std::cerr << std::endl;
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "GURU MEDITATION: Unhandeled C++ Exception (Unknown Exception Type)" << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;
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

	if(!(g_hAbortEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
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
