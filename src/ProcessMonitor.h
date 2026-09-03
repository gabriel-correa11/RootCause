#ifndef PROCESSMONITOR_H
#define PROCESSMONITOR_H

#include <Windows.h>
#include <string>
#include <vector>
#include <psapi.h>
#include <tlhelp32.h>

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

std::vector<ProcessInfo> CollectProcesses();
bool comparator(const ProcessInfo& a, const ProcessInfo& b);
#endif
