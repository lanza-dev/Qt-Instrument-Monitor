# Qt Instrument Monitor and C# Simulator

A desktop instrument monitoring demonstration built with C++/Qt Widgets
and a C#/.NET TCP server that simulates concentration measurements.

The project demonstrates asynchronous communication between separate
processes, concurrent client handling, and batch analysis on a worker thread.

## Components

- **InstrumentMonitor** — C++/Qt Widgets application for connecting,
  starting and stopping acquisition, displaying measurements, and
  analyzing collected readings.
- **InstrumentSimulator** — C#/.NET console server that generates simulated
  measurements and handles multiple clients using asynchronous I/O.
- **PowerShell test client** — Requests one measurement to test the server
  independently or while the Qt application is connected.

## Features

- TCP communication over `127.0.0.1:5000`.
- Periodic measurement requests at approximately one-second intervals.
- Connection status, socket error reporting, and a three-second timeout.
- Newline-delimited replies with numeric validation.
- Storage of up to 1,000 recent valid readings.
- Batch count, minimum, maximum, and average calculated by a worker object
  on a `QThread`.
- Queued delivery of analysis results to the UI thread.
- Cooperative cancellation and thread cleanup when the window closes.

All measurements are simulated and displayed in parts per million (ppm).

## Repository Structure

- `InstrumentMonitor/` — Qt application source and CMake configuration.
- `InstrumentSimulator/` — C# simulator source and project files.
- `scripts/Test-Instrument.ps1` — PowerShell client test.

## Requirements

- Qt 6.5 or later with Widgets and Network modules.
- CMake 3.19 or later.
- A C++ compiler compatible with the selected Qt kit.
- .NET 8 SDK.
- PowerShell for the optional client test.

The application has been exercised on Windows.

## Run the Simulator

From the repository root:

```powershell
dotnet run --project .\InstrumentSimulator\InstrumentSimulator.csproj
```

The console should display:

```text
Instrument simulator listening on 127.0.0.1:5000
```

Keep the simulator running while using the clients.

## Build and Run the Qt Application

1. Open `InstrumentMonitor/CMakeLists.txt` in Qt Creator.
2. Select a compatible desktop Qt kit with Widgets and Network installed.
3. Configure, build, and run the project.
4. Click **Connect**.
5. Click **Start Acquisition** to request and display measurements.
6. Click **Analyze Batch** to analyze a snapshot of collected readings.
7. Use **Stop Acquisition** to stop polling or **Disconnect** to close
   the connection.

## Run the PowerShell Test

With the simulator running, execute this command from the repository root:

```powershell
.\scripts\Test-Instrument.ps1
```

An example reply is:

```text
MEASUREMENT,124.6
```

The script closes its connection after receiving one reply, so the server
logs that client as disconnected.

It can run while the Qt application remains connected, demonstrating
concurrent client handling.

## Communication Protocol

Commands and replies are ASCII text terminated by a newline.

| Client command | Server reply |
|---|---|
| `READ` | `MEASUREMENT,<value>` |
| Unknown command | `ERR,UNKNOWN_COMMAND` |

The simulator formats measurements to one decimal place using a decimal point.

TCP delivers a stream of bytes. The newline identifies the end of each
application message. The Qt client processes complete lines and retains
partial incoming data in the socket buffer until a complete line is available.

## Concurrency Design

### Qt Acquisition

`QTcpSocket` signals notify the application about connection changes,
incoming data, and errors. `QTimer` triggers periodic requests and detects
timeouts. These objects operate through the UI thread's event loop.

### Batch Analysis

The application passes a snapshot of collected readings to a `BatchWorker`
object on a `QThread`. The worker calculates statistics and emits a result
signal. A queued connection delivers the result to the UI thread for display.

The statistics calculation is small. This feature demonstrates the worker
pattern, result delivery, and cooperative shutdown.

### C# Simulator

The server awaits incoming connections in an accept loop and starts an
asynchronous handler for each accepted client. Each handler reads commands
and writes replies on its own connection.

Pending asynchronous I/O allows other sessions and the accept loop to
continue. Each connection has its own handler and TCP stream.

## Manual Verification

The following scenarios were exercised during development:

- Connect, start acquisition, stop acquisition, and disconnect.
- Receive changing simulated concentration readings.
- Request a measurement through PowerShell while Qt remains connected.
- Analyze collected readings and display batch statistics.
- Close the Qt window during analysis using a temporary delay to make
  the shutdown path observable.

The temporary analysis delay was disabled after the shutdown check.

## Scope

This portfolio project uses a custom simulator protocol. Integration with
a physical instrument would require its documented commands, response
format, communication interface, and timing requirements.
