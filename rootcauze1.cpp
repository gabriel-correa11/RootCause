#include <iostream>
#include <Windows.h>
#include <stdio.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include <thread>

#pragma comment(lib, "pdh.lib")

PDH_HCOUNTER AddPDHCounter(PDH_HQUERY query, const char* counterPath)
{
	PDH_HCOUNTER counter;
	PdhAddCounterA(query, counterPath, NULL, &counter);
	return counter;
}
PDH_FMT_COUNTERVALUE GetPDHCounterValue(PDH_HCOUNTER counter)
{
	PDH_FMT_COUNTERVALUE counterValue;
	PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &counterValue);
	return counterValue;
}
struct ProcessInfo
{
	std::string name;
	DWORD pid;
	DWORD parentPid;
	SIZE_T ramMB;

};
bool comparator(const ProcessInfo& a, const ProcessInfo& b) {
	return a.ramMB > b.ramMB;
}
void threadCollector(PDH_HCOUNTER counter){
	std::cout << "thread started" << std::endl;
	double value = GetPDHCounterValue(counter).doubleValue;
	std::cout << "Value: " << value << std::endl;
}

int main()
{
	HANDLE sniffer = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 processList;
	processList.dwSize = sizeof(PROCESSENTRY32);
	BOOL bProcess = Process32First(sniffer, &processList);
	PDH_HQUERY query;
	PdhOpenQueryA(NULL, NULL, &query);
	PDH_HCOUNTER counter = AddPDHCounter(query, "\\Processor(_total)\\% Processor Time");
	PDH_HCOUNTER memCounter = AddPDHCounter(query, "\\Memory\\Page Faults/sec");
	PDH_HCOUNTER diskLatency = AddPDHCounter(query, "\\PhysicalDisk(_Total)\\Avg. Disk sec/Transfer");
	PDH_HCOUNTER diskQueue = AddPDHCounter(query, "\\PhysicalDisk(_Total)\\Avg. Disk Queue Length");
	PDH_HCOUNTER pagingUsage = AddPDHCounter(query, "\\Paging File(_Total)\\% Usage");
	PdhCollectQueryData(query);
	Sleep(1000);
	PdhCollectQueryData(query);
	double counterValue = GetPDHCounterValue(counter).doubleValue;
	double memCounterValue = GetPDHCounterValue(memCounter).doubleValue;
	double diskLatencyValue = GetPDHCounterValue(diskLatency).doubleValue;
	double diskQueueValue = GetPDHCounterValue(diskQueue).doubleValue;
	double pagingUsageValue = GetPDHCounterValue(pagingUsage).doubleValue;	
	std::cout << "CPU: " << counterValue << "% | PageFaults: " << memCounterValue
		<< " | DiskLatency: " << diskLatencyValue << "ms | DiskQueue: " << diskQueueValue
		<< " | PagingUsage: " << pagingUsageValue << "%" << std::endl;
	
	std::vector<ProcessInfo> processes;

	
	
	while (bProcess)
	{
		
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
		processes.push_back(p);
		bProcess = Process32Next(sniffer, &processList);
		
	}
	std::sort(processes.begin(), processes.end(), comparator);
	for (size_t i = 0; i < 20&& i <processes.size();i++)
	{
		std::cout << processes[i].name << " (PID: " << processes[i].pid << ") | Parent: " << processes[i].parentPid << " | RAM: " << processes[i].ramMB << "MB" << std::endl;

	}
	PdhCloseQuery(query);
	CloseHandle(sniffer);
	return 0;

}
