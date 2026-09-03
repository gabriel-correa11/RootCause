#include "PDHCollector.h"
#include <Windows.h>
#pragma comment(lib, "pdh.lib")

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
