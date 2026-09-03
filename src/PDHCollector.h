#ifndef PDHCOLLECTOR_H
#define PDHCOLLECTOR_H

#include <Windows.h>
#include <pdh.h>

struct GlobalMetrics {
    double cpuUsage;
    double pageFaults;
    double diskLatency;
    double diskQueue;
    double pagingUsage;
};

GlobalMetrics CollectMetrics();
PDH_HCOUNTER AddPDHCounter(PDH_HQUERY query, const char* counterPath);
PDH_HCOUNTER AddPDHCounterW(PDH_HQUERY query, PWSTR counterPath);
PDH_FMT_COUNTERVALUE GetPDHCounterValue(PDH_HCOUNTER counter);

#endif