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
	DWORD bufferSize = 0;
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", NULL, &bufferSize);
	PWSTR paths = (PWSTR)malloc(bufferSize * sizeof(WCHAR));
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", paths, &bufferSize);
	PWSTR EndOfPaths = paths + bufferSize;
	PDH_HQUERY query;
	PdhOpenQueryA(NULL, NULL, &query);
	for (PWSTR p = paths; ((p != EndOfPaths) && (*p != L'\0')); p += wcslen(p) + 1) {
		AddPDHCounterW(query, p);
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