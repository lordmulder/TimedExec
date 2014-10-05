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

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <ShellAPI.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 2

#define LOG_FILE "TimedExec.log"

#define EXEC_LOOPS   5
#define WARMUP_LOOPS 1

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

int _tmain(int argc, _TCHAR* argv[])
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

	TCHAR *fullCmd = GetCommandLine();
	int len = std::max(_tcslen(fullCmd) + 1U, 4096U);

	TCHAR *myCmd = new TCHAR[len];
	TCHAR *temp = new TCHAR[len];
		
	memset(myCmd, 0, sizeof(TCHAR) * len);
	memset(temp, 0, sizeof(TCHAR) * len);

	/* ---------------------------------------------------------- */
	
	int nArgs = 0;
	TCHAR **szArglist = CommandLineToArgvW(fullCmd, &nArgs);

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
		std::cerr << "  TIMED_EXEC_LOGFILE - Log File (default: \"" << LOG_FILE << "\")" << std::endl;
		std::cerr << "  TIMED_EXEC_LOOPS   - Number of execution loops (default: " << EXEC_LOOPS << ")" << std::endl;
		std::cerr << "  TIMED_WARMUP_LOOPS - Number of warm-up loops (default: " << WARMUP_LOOPS << ")\n" << std::endl;
		return EXIT_FAILURE;
	}

	for(int i = 1; i < nArgs; i++)
	{
		if(i > 1)
		{
			_tcsncat_s(myCmd, len, _T(" "), _TRUNCATE);
		}
		if(_tcschr(szArglist[i], _T(' ')) == NULL)
		{
			_tcsncat_s(myCmd, len, szArglist[i], _TRUNCATE);
		}
		else
		{
			_tcsncat_s(myCmd, len, _T("\""), _TRUNCATE);
			_tcsncat_s(myCmd, len, szArglist[i], _TRUNCATE);
			_tcsncat_s(myCmd, len, _T("\""), _TRUNCATE);
		}
	}
		
	/* ---------------------------------------------------------- */

	TCHAR *logFile = NULL;
	int maxLoops = EXEC_LOOPS, warmupLoops = WARMUP_LOOPS;
	
	if(getEnvVariable(_T("TIMED_EXEC_LOOPS"), temp, len))
	{
		int maxLoops = std::max(1, _tstoi(temp));
	}
	
	if(getEnvVariable(_T("TIMED_WARMUP_LOOPS"), temp, len))
	{
		int warmupLoops = std::max(0, _tstoi(temp));
	}

	if(getEnvVariable(_T("TIMED_EXEC_LOGFILE"), temp, len))
	{
		logFile = _tcsdup(temp);
	}
	else
	{
		logFile = _tcsdup(_T(LOG_FILE));
	}

	/* ---------------------------------------------------------- */

	_ftprintf(stderr, _T("Command-line:\n%s\n\n"), myCmd);

	const LONGLONG timerFrequency = getTimerFrequency();
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	double *singleResults = new double[maxLoops];
	memset(singleResults, 0, sizeof(double) * maxLoops);

	double slowestResult = 0.0;
	double fastestResult = DBL_MAX;

	double meanResult = 0.0;
	double variance = 0.0;

	for(int loop = 0; loop < warmupLoops; loop++)
	{
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "Warm-Up Loop " << (loop + 1) << " of " << warmupLoops << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;

		STARTUPINFO startInfo;
		SecureZeroMemory(&startInfo, sizeof(STARTUPINFO));
		PROCESS_INFORMATION processInfo;
		SecureZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

		if(!CreateProcess(NULL, myCmd, NULL, NULL, false, ABOVE_NORMAL_PRIORITY_CLASS, NULL, NULL, &startInfo, &processInfo))
		{
			std::cerr << "\nTimedExec: Failed to create process!" << std::endl;
			return EXIT_FAILURE;
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);
	}

	for(int loop = 0; loop < maxLoops; loop++)
	{
		std::cerr << "\n===========================================================================" << std::endl;
		std::cerr << "Exec Loop " << (loop + 1) << " of " << maxLoops << std::endl;
		std::cerr << "===========================================================================\n" << std::endl;

		STARTUPINFO startInfo;
		SecureZeroMemory(&startInfo, sizeof(STARTUPINFO));
		PROCESS_INFORMATION processInfo;
		SecureZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

		if(!CreateProcess(NULL, myCmd, NULL, NULL, false, ABOVE_NORMAL_PRIORITY_CLASS | CREATE_SUSPENDED, NULL, NULL, &startInfo, &processInfo))
		{
			std::cerr << "\nTimedExec: Failed to create process!" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeStart = getCurrentTime();
		
		if(ResumeThread(processInfo.hThread) == ((DWORD) -1))
		{
			std::cerr << "\nTimedExec: Failed to resume thread!" << std::endl;
			return EXIT_FAILURE;
		}
		
		if(WaitForSingleObject(processInfo.hProcess, INFINITE) != WAIT_OBJECT_0)
		{
			std::cerr << "\nTimedExec: Failed to wait for process termination!" << std::endl;
			return EXIT_FAILURE;
		}

		const LONGLONG timeFinish = getCurrentTime();
		const double execTime = static_cast<double>(timeFinish - timeStart) / static_cast<double>(timerFrequency);
		singleResults[loop] = execTime;

		std::cerr << std::setprecision(3) << std::fixed;
		std::cerr << "\nTimedExec: Execution took " << execTime << " seconds.\n" << std::endl;
		std::cerr.copyfmt(initFmt);

		if(execTime > slowestResult) slowestResult = execTime;
		if(execTime < fastestResult) fastestResult = execTime;

		const double delta = execTime - meanResult;
		meanResult += delta / double(loop + 1);
		variance += delta * (execTime - meanResult);
	}

	variance /= double(maxLoops - 1);

	/* ---------------------------------------------------------- */

	const double standardDeviation = sqrt(variance);

	std::cerr << std::setprecision(3) << std::fixed;
	std::cerr << "\n===========================================================================" << std::endl;
	std::cerr << "Average execution time after " << maxLoops << " runs was " << meanResult << " seconds." << std::endl;
	std::cerr << "Fastest / slowest execution time was " << fastestResult << " / " << slowestResult << " seconds." << std::endl;
	std::cerr << "Standard deviation was: " << standardDeviation << " seconds." << std::endl;
	std::cerr << "===========================================================================\n" << std::endl;
	std::cerr.copyfmt(initFmt);

	FILE *fLog = NULL;
	if(_tfopen_s(&fLog, logFile, _T("a+")) == 0)
	{
		_tcsncpy_s(temp, len, szArglist[1], _TRUNCATE);
		TCHAR *ctx, *exeName = _tcstok_s(temp, _T(":/\\"), &ctx);
		while(TCHAR *tok = _tcstok_s(NULL, _T(":/\\"), &ctx)) exeName = tok;
		_ftprintf_s(fLog, _T("%s\t%d\t%f\t%f\t%f\t%f\t%s\n"), exeName, maxLoops, meanResult, fastestResult, slowestResult, standardDeviation, myCmd);
		fclose(fLog); fLog = NULL;
	}
	else
	{
		std::cerr << "Error: Failed to append results to log file!\n" << std::endl;
	}

	/* ---------------------------------------------------------- */

	delete [] myCmd;
	delete [] temp;
	delete [] singleResults;

	LocalFree(szArglist);
	if(logFile) free(logFile);

	return EXIT_SUCCESS;
}

