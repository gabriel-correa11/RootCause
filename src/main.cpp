#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include "PDHCollector.h"
#include "ProcessMonitor.h"
#include "Diagnostics.h"

std::mutex mtx;

void collectMetricsThread(GlobalMetrics& metrics) {
    metrics = CollectMetrics();

    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "CPU Usage: " << metrics.cpuUsage << "%" << std::endl;
}

void collectProcessesThread(std::vector<ProcessInfo>& processes) {
    processes = CollectProcesses();

    std::lock_guard<std::mutex> lock(mtx);

    for (size_t i = 0; i < 20 && i < processes.size(); i++) {
        std::cout
            << "Process Name: " << processes[i].name
            << " | PID: " << processes[i].pid
            << " | Parent PID: " << processes[i].parentPid
            << " | RAM Usage: " << processes[i].ramMB << "MB"
            << std::endl;
    }
}

int main() {
    GlobalMetrics metrics;
    std::vector<ProcessInfo> processes;

    std::thread metricsThread(
        collectMetricsThread,
        std::ref(metrics)
    );

    std::thread processesThread(
        collectProcessesThread,
        std::ref(processes)
    );

    metricsThread.join();
    processesThread.join();
    Diagnosis diag = Analyze(metrics, processes);
    std::cout << ":  " << diag.diagnosis_verdict << std::endl;

    return 0;
}