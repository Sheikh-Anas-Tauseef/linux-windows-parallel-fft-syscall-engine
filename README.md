# Linux & Windows System Calls Demo with FFT

A small cross-platform systems programming demo that maps common Linux system calls to Windows API equivalents. Both versions read integers from input.txt, run a basic FFT, and write results to output.txt while timing key steps.

## Project Structure

```
OS CPP/
├── linux_syscalls_demo/
│   ├── input.txt
│   └── main.cpp
├── windows_syscalls_demo/
│   ├── input.txt
│   ├── main.cpp
│   └── output.txt
└── README.md
```

## System Call Mapping

| Category    | Linux                          | Windows                                          |
|-------------|-------------------------------|--------------------------------------------------|
| File I/O    | open, read, write, close       | CreateFile, ReadFile, WriteFile, CloseHandle     |
| Process     | fork, exec, wait               | CreateProcess, WaitForSingleObject               |
| Memory      | mmap, munmap                   | VirtualAlloc, VirtualFree                        |
| IPC         | pipe, shmget, shmat            | CreatePipe, CreateFileMapping, MapViewOfFile     |
| Permissions | chmod, chown, umask            | SetFileSecurity (simulated)                      |

## Requirements

### Linux
- GCC/G++
- POSIX libraries

### Windows
- MinGW or MSVC

## Build & Run

### Linux

```bash
cd linux_syscalls_demo
g++ main.cpp -o os_project
./os_project
```

### Windows (MinGW)

```bash
cd windows_syscalls_demo
g++ main.cpp -o main.exe
./main.exe
```

## Input / Output Format

Input (input.txt): one integer per line. Lines starting with # are ignored.

```
1
2
3
4
```

Output (output.txt): FFT results written as index, real, imaginary columns.

```
Index    Real       Imag
0        10.000     0.000
1        -2.000     2.000
```

Note: Input size must be a power of 2.
