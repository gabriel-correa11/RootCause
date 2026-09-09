#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "PDHCollector.h"
#include "ProcessMonitor.h"
#include <vector>
#include <string>

struct Diagnosis
{
	bool cpu_saturation;
	bool pid_isolation;
	bool memory_thrashing;
	bool io_secondary_lag;
	bool thermal_throttling;
	std::string primary_culprit_pid;
	double attribution_weight;
	std::string diagnosis_verdict;

};
Diagnosis Analyze(const GlobalMetrics& metrics,
	const std::vector<ProcessInfo>& processes);



#endif