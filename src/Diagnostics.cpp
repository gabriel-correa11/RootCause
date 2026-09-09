#include "Diagnostics.h"
#include <algorithm>

Diagnosis Analyze(const GlobalMetrics& metrics,
    const std::vector<ProcessInfo>& processes)
{
    Diagnosis diag = {};
    diag.cpu_saturation = (metrics.cpuUsage >= 90.0);
    diag.memory_thrashing = (metrics.pageFaults >= 1000.0);
    diag.io_secondary_lag = (metrics.diskLatency >= 0.1) &&(metrics.diskQueue >= 5.0);
    diag.thermal_throttling = false;
    if (diag.cpu_saturation) {
        diag.diagnosis_verdict = "CPU SATURATION";

        }
    else if (diag.memory_thrashing)
    {
        diag.diagnosis_verdict = "MEMORY_TRASHING";
    }
    else if (diag.io_secondary_lag){
        diag.diagnosis_verdict = "IO_SECUNDARY_LAG";
    
    }
    else if (diag.thermal_throttling) {
        diag.diagnosis_verdict = "THERMAL_TRHOTTLING";
    }
    else 
    {
        diag.diagnosis_verdict = "NO_ANOMALY_DETECTED";
    }
            
       

    return diag;
}