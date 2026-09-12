# RootCause

RootCause is a Windows x64 performance diagnostics application written in C++20. It collects system and process telemetry through native Windows APIs and applies deterministic rules to help investigate system slowdowns and freezes.

The application is intended to help administrators and consultants identify which resource conditions deserve further investigation. It is an individual portfolio project demonstrating Windows systems programming, concurrent data collection, and performance analysis, with professional troubleshooting use as its longer-term objective.

## Design and architecture

Collection, analysis, and orchestration are separated so that changes to diagnostic rules do not require changes to the Windows API integration.

| Module | Responsibilities | Result |
| --- | --- | --- |
| [`PDHCollector`](src/PDHCollector.cpp) | Opens PDH queries, registers system counters, collects samples, and formats counter values. | `GlobalMetrics` |
| [`ProcessMonitor`](src/ProcessMonitor.cpp) | Enumerates processes, reads working-set memory, and prepares per-process performance counters. | `std::vector<ProcessInfo>` |
| [`Diagnostics`](src/Diagnostics.cpp) | Evaluates collected measurements and selects a diagnostic verdict. | `Diagnosis` |
| [`main.cpp`](src/main.cpp) | Starts collection threads, coordinates console output, waits for results, and invokes analysis. | Console report |

```text
                    main.cpp
                       |
          +------------+------------+
          |                         |
     PDHCollector              ProcessMonitor
          |                         |
     GlobalMetrics          vector<ProcessInfo>
          |                         |
          +------------+------------+
                       |
                join both threads
                       |
                    Analyze()
                       |
                    Diagnosis
                       |
                 Console output
```

`Analyze()` receives its inputs by constant reference. Keeping Windows API calls outside the analysis function allows rules to be evaluated against supplied measurements independently of live collection.

## Telemetry collection

### System counters

Performance Data Helper (PDH) provides an interface for querying Windows performance counters. RootCause groups five counters into a query and returns their formatted values in `GlobalMetrics`.

| Windows counter | Field | Unit |
| --- | --- | --- |
| `\Processor(_total)\% Processor Time` | `cpuUsage` | Percent |
| `\Memory\Page Faults/sec` | `pageFaults` | Faults per second |
| `\PhysicalDisk(_Total)\Avg. Disk sec/Transfer` | `diskLatency` | Seconds per transfer |
| `\PhysicalDisk(_Total)\Avg. Disk Queue Length` | `diskQueue` | Average queue length |
| `\Paging File(_Total)\% Usage` | `pagingUsage` | Percent |

The collector opens a query, adds counters, and calls `PdhCollectQueryData()` twice with a one-second interval. It then reads values through `PdhGetFormattedCounterValue()` using `PDH_FMT_DOUBLE` and closes the query.

Many rate counters require two samples to calculate a meaningful value. This sequence follows Microsoft's documented procedure, which specifies waiting at least one second between collections. It establishes a sampling interval; it does not guarantee that the interval captures an intermittent slowdown. See [Collecting Performance Data](https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data).

### Process collection

`ProcessMonitor` creates a process snapshot with `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)` and traverses it with `Process32First()` and `Process32Next()`. These Toolhelp32 APIs expose executable names, PIDs, and parent PIDs. See Microsoft's [process snapshot example](https://learn.microsoft.com/en-us/windows/win32/toolhelp/taking-a-snapshot-and-viewing-processes).

For accessible processes, the collector opens a process handle and calls PSAPI's `GetProcessMemoryInfo()`. It stores `WorkingSetSize / (1024 * 1024)` in `ramMB`; this is MiB, although the console labels it MB. See [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo).

The returned vector follows process enumeration order. A descending RAM comparator exists, but no sorting call is applied. The console displays up to 20 entries, so these are not necessarily the largest memory consumers.

Per-process CPU, page-fault, and I/O collection is part of v1.5 work. The module expands wildcard counter paths through `PdhExpandCounterPathW()`, converts wide strings to UTF-8 with `WideCharToMultiByte()`, and extracts instance names. Counter values are not yet mapped into the returned process records; those measurement fields remain zero.

## Concurrency

The application launches two `std::thread` workers: one collects system metrics and the other collects process data. Each worker writes to its own result object, passed by reference.

A shared `std::mutex`, acquired with `std::lock_guard`, protects only console output. Collection happens outside that lock. Both collectors contain a one-second sampling wait, so their waits can overlap rather than execute sequentially.

The main thread calls `join()` on both workers before passing their results to `Analyze()`. This ensures that analysis starts after collection completes. The order of the two console output blocks can vary with thread scheduling.

## Diagnostic approach

The diagnostic design covers six heuristic checks. Three are implemented in the analysis function; process isolation, attribution, and thermal diagnosis require additional work.

| Check | Condition | Interpretation |
| --- | --- | --- |
| `CHECK_01: CPU_SATURATION` | CPU usage ≥ 90% | High aggregate CPU utilization during the sample. |
| `CHECK_03: MEMORY_THRASHING` | Page faults ≥ 1,000/sec | Elevated page-fault activity requiring memory investigation. |
| `CHECK_05: IO_SECONDARY_LAG` | Disk latency ≥ 0.1 seconds **and** queue length ≥ 5 | Concurrent disk delay and queue buildup. |

The function initializes a `Diagnosis` structure, evaluates the flags, and selects one verdict. CPU takes precedence over memory, followed by disk I/O. If none trigger, the result is `NO_ANOMALY_DETECTED`. Paging file utilization is collected but does not participate in these rules.

### Threshold rationale

The thresholds are explicit project heuristics informed by performance troubleshooting concepts. The references below explain the relevant measurements; they do not validate the exact combination of RootCause thresholds as a Microsoft benchmark.

- **90% CPU:** the rule flags operation near full utilization. Microsoft's Windows Server guidance discusses sustained usage of 80% or higher and distinguishes it from temporary spikes. RootCause's 90% cutoff is a project choice, and one sample does not establish sustained saturation. See [High CPU usage troubleshooting guidance](https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/troubleshoot-high-cpu-usage-guidance).
- **1,000 page faults/sec:** this provides an initial trigger for investigating memory activity. Page faults include soft and hard faults, so the threshold alone does not prove memory thrashing or disk-backed paging. No verified Microsoft reference establishes this value as a universal Windows memory-pressure limit. See [An Overview of Troubleshooting Memory Issues](https://techcommunity.microsoft.com/blog/askperf/an-overview-of-troubleshooting-memory-issues---part-two/372679).
- **100 ms disk latency and queue length 5:** requiring both conditions combines service delay with queued activity. These cutoffs need validation against the storage device and workload. Microsoft's SQL Server guidance uses much lower sustained latency figures, around 10–15 ms in that context, and explains that expectations depend on the system. It does not establish RootCause's 100 ms/5 combination. See [Troubleshoot slow SQL Server performance caused by I/O issues](https://learn.microsoft.com/en-us/troubleshoot/sql/database-engine/performance/troubleshoot-sql-io-performance).

Deterministic rules produce the same result for the same inputs and make each decision inspectable. They can still produce false positives or miss a problem. A verdict is an investigation signal, not proof of a root cause.

## Technical choices

| Choice | Rationale and tradeoff |
| --- | --- |
| C++20 and MSVC | Direct access to native Windows APIs and explicit control of handles, buffers, and resource lifetimes. This also requires careful error handling and cleanup. |
| PDH | Provides the performance counter query and formatting interfaces used by the collectors. No PDH-versus-WMI speed benchmark has been established for this project. |
| Toolhelp32 and PSAPI | Separate process identity enumeration from process memory inspection using APIs suited to each task. |
| Modular implementation | Keeps measurement collection separate from interpretation and output coordination. |
| Deterministic rules | Requires no training data or model inference. Conditions are visible in source, but threshold quality still depends on validation. |
| Concurrent collection | Allows independent collection work and sampling waits to overlap. No measured runtime improvement is claimed. |

C++ was chosen over a Python or C# implementation to focus the project on native systems programming. Portable executable distribution remains a packaging goal; runtime dependencies must be checked before distribution.

## Build and run

### Requirements

- Windows x64.
- Visual Studio 2022 with **Desktop development with C++**, MSVC, and the Windows SDK.
- CMake and Ninja, available through Visual Studio's CMake tools or separate installations.

### Command line

Open an **x64 Native Tools Command Prompt for Visual Studio** and run these commands from the repository root:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\rootcauze.exe
```

The CMake target is named `rootcauze`. With the single-configuration **Ninja** generator above, the executable is `build\rootcauze.exe`, not `build\Debug\rootcauze.exe`. Use a fresh build directory if `build` is already configured with a different generator.

### Visual Studio

Open the repository as a CMake folder, select the `x64-debug` preset, and build and run the `rootcauze` target. The supplied Windows presets use Ninja and include a Visual Studio-specific CMake integration path. Linux and macOS preset entries do not make the Windows source portable to those platforms.

The instructions reflect the checked-in build configuration. A clean build and runtime test were not performed for this documentation update.

### Console output

Illustrative output matching the current format, with invented measurements:

```text
CPU Usage: 87.3%
Process Name: devenv.exe | PID: 14092 | Parent PID: 10476 | RAM Usage: 1240MB
Process Name: svchost.exe | PID: 1724 | Parent PID: 1536 | RAM Usage: 34MB
...
:  MEMORY_TRASHING
```

The verdict spelling above matches the source. The intended expanded report format is:

```text
Verdict: MEMORY_THRASHING
Culprit: NONE
Weight: 0.0
```

The expanded labels, culprit, and weight are not printed by the current implementation. The memory verdict depends on the collected page-fault rate, which is not shown in this example.

## Known limitations

The v1.0 diagnostic core includes global counter collection, process enumeration and memory reads, concurrent execution, and three rule checks. Its main constraints are:

- **Single collection pass:** there is no continuous monitoring or history to distinguish transient spikes from sustained conditions.
- **No process attribution:** `Analyze()` accepts process records but does not use them to identify a culprit. Attribution fields remain at their initialized defaults.
- **Incomplete per-process counters:** CPU, page-fault, and I/O values are not populated. Instance identity and processor-count normalization require care; Microsoft documents these issues in [Collecting Performance Data](https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data).
- **Unsorted process output:** enumeration order is preserved, and output is limited to 20 entries.
- **Limited failure handling:** API return values, invalid samples, unavailable counters, and inaccessible process memory need more robust handling. English counter paths also require validation on localized Windows installations.
- **No thermal measurements:** the thermal flag is explicitly false; the application cannot infer thermal throttling from the available data.
- **Unvalidated diagnostic accuracy:** no automated test suite or representative workload benchmark is included. Aggregate counters can hide differences between individual disks or processors.

`NO_ANOMALY_DETECTED` means that none of the implemented conditions triggered. It does not establish that the system is healthy or that the sample data is valid.
## References

- [Collecting Performance Data — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data): two-sample collection, sampling interval, and process counter interpretation.
- [Taking a snapshot and viewing processes — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/toolhelp/taking-a-snapshot-and-viewing-processes): Toolhelp32 enumeration and process access restrictions.
- [GetProcessMemoryInfo — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo): PSAPI memory queries.
- [High CPU usage troubleshooting guidance — Microsoft Learn](https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/troubleshoot-high-cpu-usage-guidance): sustained CPU usage and investigation methods.
- [An Overview of Troubleshooting Memory Issues — Microsoft](https://techcommunity.microsoft.com/blog/askperf/an-overview-of-troubleshooting-memory-issues---part-two/372679): memory counters and soft versus hard page faults.
- [Troubleshoot slow SQL Server performance caused by I/O issues — Microsoft Learn](https://learn.microsoft.com/en-us/troubleshoot/sql/database-engine/performance/troubleshoot-sql-io-performance): disk latency interpretation and workload-dependent thresholds.
