# RootCause

RootCause is a Windows performance analysis application written in C++20. It collects system and process telemetry through native Windows APIs and applies deterministic rules to identify signs of CPU saturation, memory pressure, and disk I/O contention.

I started this individual project during an internship with a systems consultant to investigate Windows slowdowns and freezes. It is also part of my portfolio for a master's application, bringing together systems programming, concurrent collection, and performance analysis.

## Design

The application separates telemetry collection from diagnostic logic. Collectors produce structured data, and the analysis function evaluates that data without making Windows API calls itself.

| Module | Responsibility |
| --- | --- |
| `PDHCollector` | Manages PDH queries and reads system-wide performance counters into `GlobalMetrics`. |
| `ProcessMonitor` | Enumerates processes through Toolhelp32, reads working-set memory through PSAPI, and handles process counter paths. |
| `Diagnostics` | Evaluates measurements through `Analyze()` and returns a `Diagnosis` structure. |
| `main.cpp` | Coordinates collection threads and console output, then invokes the analysis. |

The main data types are defined alongside their modules:

- `GlobalMetrics`: CPU utilization, page-fault rate, disk latency, disk queue length, and paging file usage.
- `ProcessInfo`: process identity and resource measurement fields.
- `Diagnosis`: diagnostic flags, attribution fields, and the selected verdict.

Field definitions describe the data model; the collection and analysis implementations determine which fields are populated and evaluated.

## Telemetry collection

### System counters

`PDHCollector` uses Performance Data Helper (PDH) to open a query, register counters, collect samples, and retrieve formatted values.

| Counter | Measurement |
| --- | --- |
| `\Processor(_total)\% Processor Time` | Total CPU utilization (%) |
| `\Memory\Page Faults/sec` | System page-fault rate |
| `\PhysicalDisk(_Total)\Avg. Disk sec/Transfer` | Average disk transfer latency in seconds |
| `\PhysicalDisk(_Total)\Avg. Disk Queue Length` | Average disk queue length |
| `\Paging File(_Total)\% Usage` | Paging file utilization (%) |

The collection sequence calls `PdhCollectQueryData()` twice with a one-second interval, then reads the counters using `PdhGetFormattedCounterValue()` with `PDH_FMT_DOUBLE`. The query is closed after the values have been extracted.

### Process data

`ProcessMonitor` uses `CreateToolhelp32Snapshot()` with `TH32CS_SNAPPROCESS`, then traverses the snapshot using `Process32First()` and `Process32Next()`.

Process records include the executable name, PID, and parent PID. `OpenProcess()` provides a handle for reading memory information through `GetProcessMemoryInfo()`. Working-set size is converted from bytes to MiB using `1024 * 1024`.

The module also uses `PdhExpandCounterPathW()` to expand wildcard paths for process CPU time, page faults, and I/O throughput. These paths are returned as wide strings. `WideCharToMultiByte()` converts them to UTF-8, and string parsing extracts the process-instance name for counter bookkeeping.

## Concurrency

`main.cpp` starts one `std::thread` for system metrics and another for process collection. Each worker writes to a separate result object passed by reference, allowing the collection work and sampling intervals to overlap.

Console writes are protected by `std::lock_guard<std::mutex>`. The lock covers output rather than the collection phase. The main thread calls `join()` on both workers before reading their results and invoking `Analyze()`.

This keeps synchronization at the points where it is needed: shared output and the transition from collection to analysis.

## Diagnostic approach

RootCause uses explicit threshold rules. This makes each result traceable to a condition in the source and reproducible for the same input measurements, without a training dataset or model inference step.

`Analyze()` accepts the collected data by constant reference and returns a separate result structure. Threshold comparisons set diagnostic flags; an ordered conditional chain selects the printed verdict. The exact conditions and their precedence are defined in [`src/Diagnostics.cpp`](src/Diagnostics.cpp).

The rules indicate conditions worth investigating. They do not establish causality on their own. In particular, page faults include both soft and hard faults, so a page-fault rate alone cannot establish disk-backed memory thrashing. Microsoft's [memory troubleshooting reference](https://techcommunity.microsoft.com/blog/askperf/an-overview-of-troubleshooting-memory-issues---part-two/372679) explains the distinction.

Separating this logic from collection allows diagnostic conditions to be evaluated using supplied measurements and revised independently of the Windows API integration.

## Technical choices

I chose C++ to work directly with Windows APIs and control the lifetime of queries, process handles, and buffers. The project uses C++20 with CMake and MSVC.

PDH provides the performance counter interface, Toolhelp32 provides process enumeration, and PSAPI provides process memory information. Each API serves a specific part of the collection pipeline.

The module boundaries reflect those responsibilities. Changes to counter collection belong in the collectors; interpretation belongs in `Diagnostics`; execution order and output coordination belong in `main.cpp`.

## Build and run

Requirements:

- Windows with MSVC and the Windows SDK.
- CMake and Ninja.
- For Visual Studio, the **Desktop development with C++** workload and CMake tools.

Open the repository folder in Visual Studio and select the `x64-debug` preset, or run the following from an **x64 Native Tools Command Prompt for Visual Studio**:

```bat
cmake -S . -B out/build/manual-x64 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/manual-x64
.\out\build\manual-x64\rootcauze.exe
```

The CMake target and executable are named `rootcauze`. The application depends on Windows APIs; the Linux and macOS preset entries do not provide platform compatibility.

## Development background

I implemented the application as an individual project, working through PDH queries, process enumeration, memory measurements, threading, and diagnostic rules. The work involved handling Windows string representations, parsing counter paths, managing API resources, and debugging type mismatches and counter bookkeeping.

I used Claude for API explanations, architecture discussions, documentation research, and debugging guidance. Codex assisted with the README using my project notes and the source code.
