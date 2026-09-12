#include "ProcessMonitor.h"
#include "PDHCollector.h"
#include <pdh.h>

bool Comparator(const ProcessInfo& a, const ProcessInfo& b) {
	return a.ramMB > b.ramMB;
}

std::vector<ProcessInfo> CollectProcesses() {
	HANDLE sniffer = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 processList;
	processList.dwSize = sizeof(PROCESSENTRY32);
	BOOL bProcess = Process32First(sniffer, &processList);
	std::vector<ProcessInfo> processes;
	DWORD bufferSizeCPU = 0;
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", NULL, &bufferSizeCPU);
	PWSTR pathsCPU = (PWSTR)malloc(bufferSizeCPU * sizeof(WCHAR));
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", pathsCPU, &bufferSizeCPU);
	DWORD bufferSizeFaults = 0;
	PdhExpandCounterPathW(L"\\Process(*)\\Page Faults/sec", NULL, &bufferSizeFaults);
	PWSTR pathsFaults = (PWSTR)malloc(bufferSizeFaults * sizeof(WCHAR));
	PdhExpandCounterPathW(L"\\Process(*)\\Page Faults/sec", pathsFaults, &bufferSizeFaults);
	DWORD bufferSizeIO = 0;
	PdhExpandCounterPathW(L"\\Process(*)\\IO Data Bytes/sec", NULL, &bufferSizeIO);
	PWSTR pathsIO = (PWSTR)malloc(bufferSizeIO * sizeof(WCHAR));
	PdhExpandCounterPathW(L"\\Process(*)\\IO Data Bytes/sec", pathsIO, &bufferSizeIO);
	PWSTR EndOfPathsCPU = pathsCPU + bufferSizeCPU;
	PWSTR EndOfPathsFaults = pathsFaults + bufferSizeFaults;
	PWSTR EndOfPathsIO = pathsIO + bufferSizeIO;

	struct CounterInfo
	{
		PDH_HCOUNTER handle;
		std::string processName;
	};

	PDH_HQUERY query;
	std::vector<CounterInfo> cpucounters;
	std::vector<CounterInfo> faultscounters;
	std::vector<CounterInfo> iocounters;

	PdhOpenQueryA(NULL, NULL, &query);

	for (PWSTR p = pathsCPU; p < EndOfPathsCPU; p += wcslen(p) + 1) {
		PDH_HCOUNTER handle = AddPDHCounterW(query, p);

		int len = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
		std::string path(len - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, p, -1, &path[0], len, NULL, NULL);

		size_t start = path.find('(') + 1;
		size_t end = path.find(')');
		std::string processName = path.substr(start, end - start);

		CounterInfo info;
		info.handle = handle;
		info.processName = processName;
		cpucounters.push_back(info);
	}

	for (PWSTR p = pathsFaults; p < EndOfPathsFaults; p += wcslen(p) + 1) {
		PDH_HCOUNTER handle = AddPDHCounterW(query, p);
		int len = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
		std::string path(len - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, p, -1, &path[0], len, NULL, NULL);

		size_t start = path.find('(') + 1;
		size_t end = path.find(')');
		std::string processName = path.substr(start, end - start);

		CounterInfo info;
		info.handle = handle;
		info.processName = processName;
		iocounters.push_back(info);
	}

	for (PWSTR p = pathsIO; p < EndOfPathsIO; p += wcslen(p) + 1) {
		PDH_HCOUNTER handle = AddPDHCounterW(query, p);
		int len = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
		std::string path(len - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, p, -1, &path[0], len, NULL, NULL);

		size_t start = path.find('(') + 1;
		size_t end = path.find(')');
		std::string processName = path.substr(start, end - start);

		CounterInfo info;
		info.handle = handle;
		info.processName = processName;
		faultscounters.push_back(info);
	}

	PdhCollectQueryData(query);
	Sleep(1000);
	PdhCollectQueryData(query);
	while (bProcess) {
		HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processList.th32ProcessID);
		if (processHandle == NULL)
		{
			//std::cout << "Error: " << GetLastError() << std::endl;
			bProcess = Process32Next(sniffer, &processList);
			continue;

		}
		PROCESS_MEMORY_COUNTERS pmc;
		GetProcessMemoryInfo(processHandle, &pmc, sizeof(pmc));
		CloseHandle(processHandle);
		ProcessInfo p;
		p.name = processList.szExeFile;
		p.pid = processList.th32ProcessID;
		p.parentPid = processList.th32ParentProcessID;
		p.ramMB = pmc.WorkingSetSize / (1024 * 1024);
		p.cpuUsagePercent = 0.0;
		p.hardPageFaultsPerSec = 0.0;
		p.ioBytesTotal = 0;
		processes.push_back(p);
		bProcess = Process32Next(sniffer, &processList);

	}
	CloseHandle(sniffer);
	free(pathsCPU);
	free(pathsFaults);
	free(pathsIO);
	PdhCloseQuery(query);
	return processes;
}