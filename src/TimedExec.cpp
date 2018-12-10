//////////////////////////////////////////////////////////////////////////////////
// Timed Exec - Command-Line Benchmarking Utility
// Copyright (c) 2018 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.
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
#include <Shlwapi.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 4

#define DEFAULT_EXEC_LOOPS 5
#define DEFAULT_WARMUP_LOOPS 1
#define DEFAULT_LOGFILE "TimedExec.log"
#define ENABLE_ENV_VARS true

#ifdef _UNICODE
#define tstring std::wstring
#else
#define tstring std::string
#endif

static HANDLE g_hAbortEvent = NULL;
static volatile bool g_aborted = false;

// =============================================================================================================
// INTERNAL FUNCTIONS
// =============================================================================================================

static bool getEnvVariable(const _TCHAR *const name, tstring &value)
{
	std::vector<TCHAR> buffer;

	for (int i = 0; i < 3; ++i)
	{
		const DWORD result = GetEnvironmentVariable(name, buffer.data(), (DWORD)buffer.size());
		if (!result)
		{
			break; /*failed*/
		}
		if (result > buffer.size())
		{
			buffer.resize(result); /*adjust buffer*/
		}
		else if (result < buffer.size())
		{
			value = tstring(buffer.data());
			return true;
		}
	}

	value.clear();
	return false;
}

static LONGLONG getCurrentTime(void)
{
	LARGE_INTEGER timeValue;
	if(!QueryPerformanceCounter(&timeValue))
	{
		std::cerr << "\n\nSYSTEM ERROR: Failed to query performance counter!\n" << std::endl;
		_exit(EXIT_FAILURE);
	}
	return timeValue.QuadPart;
}

static LONGLONG getTimerFrequency(void)
{
	LARGE_INTEGER timeValue;
	if(!QueryPerformanceFrequency(&timeValue))
	{
		std::cerr << "\n\nSYSTEM ERROR: Failed to query performance counter frequency!\n" << std::endl;
		_exit(EXIT_FAILURE);
	}
	return timeValue.QuadPart;
}

static long long getCurrentFileSize(FILE *const filePtr)
{
	struct _stati64 stats;
	if(_fstati64(_fileno(filePtr), &stats) == 0)
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
	std::vector<TCHAR> buffer;

	for (int i = 0; i < 3; ++i)
	{
		const DWORD result = GetFullPathName(fileName, (DWORD)buffer.size(), buffer.data(), NULL);
		if (!result)
		{
			break; /*failed*/
		}
		if (result > buffer.size())
		{
			buffer.resize(result); /*adjust buffer*/
		}
		else if (result < buffer.size())
		{
			return tstring(buffer.data());
		}
	}

	return tstring(fileName);
}

static tstring getFileNameOnly(const tstring &filePath)
{
	TCHAR *buffer = _tcsdup(filePath.c_str());
	if (!buffer)
	{
		return tstring();
	}

	PathStripPath(buffer);

	const tstring result(buffer);
	free(buffer);
	return result;
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

static int initializeCommandLine(tstring &commandLine, tstring &programFile)
{
	commandLine.clear();
	programFile.clear();

	int nArgs = 0;
	TCHAR **szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

	if((szArglist == NULL) || (nArgs < 2))
	{
		return 0;
	}

	for(int i = 1; i < nArgs; i++)
	{
		tstring token;

		if(i > 1)
		{
			token = tstring(szArglist[i]);
			commandLine += _T(' ');
		}
		else
		{
			token = getFullPath(szArglist[i]);
			programFile += token;
		}

		if(token.find(' ') == tstring::npos)
		{
			commandLine += token;
		}
		else
		{
			commandLine += _T('"');
			commandLine += token;
			commandLine += _T('"');
		}
	}

	LocalFree(szArglist);
	return nArgs;
}

static bool createProcess(const tstring &commandLine, HANDLE &hThrd, HANDLE &hProc, const bool suspended = false)
{
	STARTUPINFO startInfo;
	SecureZeroMemory(&startInfo, sizeof(STARTUPINFO));
	PROCESS_INFORMATION processInfo;
	SecureZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

	if(!CreateProcess(NULL, (LPTSTR)commandLine.c_str(), NULL, NULL, false, HIGH_PRIORITY_CLASS | (suspended ? CREATE_SUSPENDED : 0), NULL, NULL, &startInfo, &processInfo))
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

static int getProcessExitCode(const HANDLE &hProc)
{
	DWORD exitCode;
	if(GetExitCodeProcess(hProc, &exitCode))
	{
		return *reinterpret_cast<int*>(&exitCode);
	}

	return 0;
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

static int timedExecMain(int argc, _TCHAR* argv[])
{
	std::ios initFmt(NULL);
	initFmt.copyfmt(std::cerr);

	std::cerr << "\n===============================================================================" << std::endl;
	std::cerr << "Timed Exec - Benchmarking Utility, Version " << VERSION_MAJOR << '.' << std::setfill('0') << std::setw(2) << VERSION_MINOR << " [" __DATE__ "]" << std::endl;
	std::cerr << "Copyright (c) 2018 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n" << std::endl;
	std::cerr << "This program is free software: you can redistribute it and/or modify" << std::endl;
	std::cerr << "it under the terms of the GNU General Public License <http://www.gnu.org/>." << std::endl;
	std::cerr << "Note that this program is distributed with ABSOLUTELY NO WARRANTY." << std::endl;
	std::cerr << "===============================================================================\n" << std::endl;

	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Check Command-Line                                         */
	/* ---------------------------------------------------------- */

	tstring commandLine, programFile;
	if(initializeCommandLine(commandLine, programFile) < 2)
	{
		std::cerr << "Usage:" << std::endl;
		std::cerr << "  TimedExec.exe <Program.exe> [Arguments]\n" << std::endl;
		std::cerr << "Influential environment variables:" << std::endl;
		std::cerr << "  TIMED_EXEC_PASSES        - Number of execution passes (default: " << DEFAULT_EXEC_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_EXEC_WARMUP_PASSES - Number of warm-up passes (default: " << DEFAULT_WARMUP_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_EXEC_LOGFILE       - Log-File Name (default: \"" << DEFAULT_LOGFILE << "\")" << std::endl;
		std::cerr << "  TIMED_EXEC_NO_CHECKS     - Set this to *disable* exit code checks\n" << std::endl;
		return EXIT_FAILURE;
	}

	if(_taccess(programFile.c_str(), 0) != 0)
	{
		_ftprintf(stderr, _T("Specified program file could not be found or access denied:\n%s\n\n"), programFile.c_str());
		return EXIT_FAILURE;
	}

	if(!checkBinary(programFile))
	{
		_ftprintf(stderr, _T("Specified file does not look like a valid Win32 executable:\n%s\n\n"), programFile.c_str());
		return EXIT_FAILURE;
	}

	/* ---------------------------------------------------------- */
	/* Check Environment Variables                                */
	/* ---------------------------------------------------------- */

	tstring logFile(getFullPath(_T(DEFAULT_LOGFILE)));
	int maxPasses = DEFAULT_EXEC_LOOPS, maxWarmUpPasses = DEFAULT_WARMUP_LOOPS;
	bool checkExitCodes = true;
	
	if(ENABLE_ENV_VARS)
	{
		tstring temp;
		if(getEnvVariable(_T("TIMED_EXEC_PASSES"), temp))
		{
			maxPasses = std::min(SHRT_MAX, std::max(3, _tstoi(temp.c_str())));
		}
		if(getEnvVariable(_T("TIMED_EXEC_WARMUP_PASSES"), temp))
		{
			maxWarmUpPasses = std::min(SHRT_MAX, std::max(0, _tstoi(temp.c_str())));
		}
		if(getEnvVariable(_T("TIMED_EXEC_LOGFILE"), temp))
		{
			logFile = getFullPath(temp.c_str());
		}
		if(getEnvVariable(_T("TIMED_EXEC_NO_CHECKS"), temp))
		{
			checkExitCodes = (_tstoi(temp.c_str()) == 0);
		}
	}

	/* ---------------------------------------------------------- */
	/* Initialization                                             */
	/* ---------------------------------------------------------- */

	_ftprintf(stderr, _T("Command-line:\n%s\n\n"), commandLine.c_str());
	_ftprintf(stderr, _T("Log File:\n%s\n\n"), logFile.c_str());
	std::cerr << "Warm-Up / Metering passes: " << maxWarmUpPasses << "x / " << maxPasses << 'x' << std::endl;

	const LONGLONG timerFrequency = getTimerFrequency();
	if(!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS))
	{
		if(!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
		{
			std::cerr << "\nWARNING: Failed to set process priroity class!" << std::endl;
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

	for(int pass = 0; pass < maxWarmUpPasses; pass++)
	{
		std::cerr << "\n===============================================================================" << std::endl;
		if (maxWarmUpPasses > 1) std::cerr << "WARM-UP PASS " << (pass + 1) << " OF " << maxWarmUpPasses << std::endl; else std::cerr << "WARM-UP PASS" << std::endl;
		std::cerr << "===============================================================================\n" << std::endl;

		HANDLE hThrd, hProc;

		if(!createProcess(commandLine, hThrd, hProc))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to create process!\n" << std::endl;
			return EXIT_FAILURE;
		}

		if(!waitForProcess(hProc))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to wait for process termination!\n" << std::endl;
			return EXIT_FAILURE;
		}

		if(g_aborted)
		{
			abortedHandlerRoutine(hProc);
			return EXIT_FAILURE;
		}

		const int exitCode = getProcessExitCode(hProc);
		if(checkExitCodes && (exitCode != 0))
		{
			std::cerr << "\n\nPROGRAM ERROR: Abnormal program termination detected! (Exit Code: " << exitCode << ")\n" << std::endl;
			return EXIT_FAILURE;
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

		if(!createProcess(commandLine, hThrd, hProc, true))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to create process!\n" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeStart = getCurrentTime();
		
		if(ResumeThread(hThrd) == ((DWORD) -1))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to resume child process!\n" << std::endl;
			TerminateProcess(hProc, UINT(-1));
			return EXIT_FAILURE;
		}
		
		if(!waitForProcess(hProc))
		{
			std::cerr << "\n\nSYSTEM ERROR: Failed to wait for process termination!\n" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeFinish = getCurrentTime();
		const double execTime = static_cast<double>(timeFinish - timeStart) / static_cast<double>(timerFrequency);

		if(g_aborted)
		{
			abortedHandlerRoutine(hProc);
			return EXIT_FAILURE;
		}

		const int exitCode = getProcessExitCode(hProc);
		if(checkExitCodes && (exitCode != 0))
		{
			std::cerr << "\n\nPROGRAM ERROR: Abnormal program termination detected! (Exit Code: " << exitCode << ")\n" << std::endl;
			return EXIT_FAILURE;
		}
		else
		{
			std::cerr << "\n>> Process terminated with exit code " << exitCode << '.' << std::endl;
		}

		std::cerr << std::setprecision(3) << std::fixed;
		std::cerr << ">> Execution took " << execTime << " seconds." << std::endl;
		std::cerr.copyfmt(initFmt);

		CloseHandle(hThrd);
		CloseHandle(hProc);

		// Store this sample
		stats_samples[pass] = execTime;

		// Update slowest/fastest
		if(execTime > stats_slowest) stats_slowest = execTime;
		if(execTime < stats_fastest) stats_fastest = execTime;

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
	std::cerr << "===============================================================================\n" << std::endl;
	std::cerr.copyfmt(initFmt);

	/* ---------------------------------------------------------- */
	/* Write Log-File                                             */
	/* ---------------------------------------------------------- */

	FILE *fLog = NULL;
	if(_tfopen_s(&fLog, logFile.c_str(), _T("a+")) == 0)
	{
		if(getCurrentFileSize(fLog) == 0)
		{
			_ftprintf_s(fLog, _T("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n"), _T("Program"), _T("Passes"), _T("Mean Time"), _T("Median Time"), _T("90% Confidence Interval"), _T("95% Confidence Interval"), _T("99% Confidence Interval"), _T("Fastest Pass"), _T("Slowest Pass"), _T("Standard Deviation"), _T("Standard Error"), _T("Command Line"));
		}
		_ftprintf_s(fLog, _T("%s\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%s\n"), getFileNameOnly(programFile).c_str(), maxPasses, stats_mean, medianTime, confidenceInterval_90, confidenceInterval_95, confidenceInterval_99, stats_fastest, stats_slowest, standardDeviation, standardError, commandLine.c_str());
		if(ferror(fLog) == 0)
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
