#define WIN32_LEAN_AND_MEAN 1
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
#include <mutex>

#pragma comment(lib, "pdh.lib")
struct ProcessInfo
{
	std::string name;
	DWORD pid;
	DWORD parentPid;
	SIZE_T ramMB;
	double cpuUsagePercent;
	double hardPageFaultsPerSec;
	SIZE_T ioBytesTotal;
};
struct GlobalMetrics
{
	double cpuUsage;
	double pageFaults;
	double diskLatency;
	double diskQueue;
	double pagingUsage;

};

PDH_HCOUNTER AddPDHCounter(PDH_HQUERY query, const char* counterPath)
{
	PDH_HCOUNTER counter;
	PdhAddCounterA(query, counterPath, NULL, &counter);
	return counter;
}
PDH_HCOUNTER AddPDHCounterW(PDH_HQUERY query, const PWSTR counterPath) {
	PDH_HCOUNTER counter;
	PdhAddCounterW(query, counterPath, NULL, &counter);
	return counter;
}
PDH_FMT_COUNTERVALUE GetPDHCounterValue(PDH_HCOUNTER counter)
{
	PDH_FMT_COUNTERVALUE counterValue;
	PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &counterValue);
	return counterValue;
}

bool comparator(const ProcessInfo& a, const ProcessInfo& b) {
	return a.ramMB > b.ramMB;
}
void threadCollector(PDH_HCOUNTER counter) {
	std::cout << "thread started" << std::endl;
	double value = GetPDHCounterValue(counter).doubleValue;
	std::cout << "Value: " << value << std::endl;
}

GlobalMetrics CollectMetrics() {

	GlobalMetrics metrics;
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
	metrics.cpuUsage = GetPDHCounterValue(counter).doubleValue;
	metrics.pageFaults = GetPDHCounterValue(memCounter).doubleValue;
	metrics.diskLatency = GetPDHCounterValue(diskLatency).doubleValue;
	metrics.diskQueue = GetPDHCounterValue(diskQueue).doubleValue;
	metrics.pagingUsage = GetPDHCounterValue(pagingUsage).doubleValue;
	PdhCloseQuery(query);
	return metrics;
}
std::vector<ProcessInfo> collectProcesses() {
	HANDLE sniffer = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 processList;
	processList.dwSize = sizeof(PROCESSENTRY32);
	BOOL bProcess = Process32First(sniffer, &processList);
	std::vector<ProcessInfo> processes;
	DWORD bufferSize = 0;
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", NULL, &bufferSize );
	PWSTR paths = (PWSTR)malloc(bufferSize * sizeof(WCHAR));
	PdhExpandCounterPathW(L"\\Process(*)\\% Processor Time", paths, &bufferSize );
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


	return processes;
}
std::mutex mtx;

void collectMetricsThread() {

	GlobalMetrics metrics = CollectMetrics();
	std::lock_guard<std::mutex> lock(mtx);
	std::cout << "CPU Usage: " << metrics.cpuUsage << "%" << std::endl;


}
void collectProcessesThread() {
	std::vector<ProcessInfo> processes = collectProcesses();
	std::lock_guard<std::mutex> lock(mtx);
	for (size_t i = 0; i < 20 && i < processes.size(); i++)
	{
		std::cout << "Process Name: " << processes[i].name << " | PID: " << processes[i].pid << " | Parent PID: " << processes[i].parentPid << " | RAM Usage: "
			<< processes[i].ramMB << "MB" << std::endl;
	}
}


int main()
{
	std::thread metricsThread(collectMetricsThread);
	std::thread processesThread(collectProcessesThread);
	metricsThread.join();
	processesThread.join();

	GlobalMetrics metrics = CollectMetrics();
	std::vector<ProcessInfo> processes = collectProcesses();
	std::cout << "CPU: " << metrics.cpuUsage << "% | PageFaults: " << metrics.pageFaults
		<< " | DiskLatency: " << metrics.diskLatency << "ms | DiskQueue: " << metrics.diskQueue
		<< " | PagingUsage: " << metrics.pagingUsage << "%" << std::endl;
	std::sort(processes.begin(), processes.end(), comparator);
	for (size_t i = 0; i < 20 && i < processes.size();i++)
	{
		std::cout << processes[i].name << " (PID: " << processes[i].pid << ") | Parent: " << processes[i].parentPid << " | RAM: " << processes[i].ramMB << "MB" << std::endl;

	}

	return 0;
}