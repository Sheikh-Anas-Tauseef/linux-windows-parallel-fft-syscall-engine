// Windows version of Linux system calls FFT project
// Compile with: g++ main.cpp -o os_windows -luser32 -lkernel32
// This code mirrors the Linux version, replacing system calls with WinAPI equivalents.

#include <windows.h> // WinAPI
#include <iostream>
#include <vector>
#include <sstream>
#include <complex>
#include <cmath>
#include <string>

#define INPUT_FILE "input.txt"
#define OUTPUT_FILE "output.txt"
#define SHM_SIZE 2048 // bytes (enough for 100 doubles)
#define MAX_INTS 100

// --- FFT Implementation (unchanged) ---
void fft(std::vector<std::complex<double>>& a) {
    int n = a.size();
    if (n <= 1) return;
    std::vector<std::complex<double>> even(n / 2), odd(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        even[i] = a[i * 2];
        odd[i] = a[i * 2 + 1];
    }
    fft(even);
    fft(odd);
    for (int k = 0; k < n / 2; ++k) {
        std::complex<double> t = std::polar(1.0, -2 * M_PI * k / n) * odd[k];
        a[k] = even[k] + t;
        a[k + n / 2] = even[k] - t;
    }
}

bool is_power_of_2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// Timing helpers
LONGLONG qpc_freq = 0;
LONGLONG qpc_now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
double qpc_seconds(LONGLONG start, LONGLONG end) {
    return double(end - start) / double(qpc_freq);
}

void print_win_error(const char* msg) {
    DWORD err = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf, 0, NULL);
    std::cerr << msg << ": " << (lpMsgBuf ? (char*)lpMsgBuf : "Unknown error") << std::endl;
    if (lpMsgBuf) LocalFree(lpMsgBuf);
}

int main() {
    // --- Timing: Start total execution timer ---
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    qpc_freq = freq.QuadPart;
    LONGLONG total_start = qpc_now();

    // Simulate umask (not needed on Windows)
    std::cout << "[INFO] (Simulated) Setting default permissions\n";

    // --- Timing: Start file read timer ---
    LONGLONG read_start = qpc_now();

    // Linux: open() -> Windows: CreateFile()
    std::cout << "[INFO] Opening input file (CreateFile)\n";
    HANDLE hFile = CreateFileA(INPUT_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        print_win_error("Error opening input.txt");
        return 1;
    }

    // Linux: read() -> Windows: ReadFile()
    std::cout << "[INFO] Reading input file (ReadFile)\n";
    char buffer[4096];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        print_win_error("Error reading input.txt");
        CloseHandle(hFile);
        return 1;
    }
    buffer[bytesRead] = '\0';

    // Linux: close() -> Windows: CloseHandle()
    std::cout << "[INFO] Closing input file (CloseHandle)\n";
    CloseHandle(hFile);

    // --- Timing: End file read timer ---
    LONGLONG read_end = qpc_now();
    double file_read_time = qpc_seconds(read_start, read_end);
    std::cout << "[INFO] File Read Time: " << file_read_time << " seconds\n";

    // Parse buffer line-by-line, extract only integers, skip comments
    std::vector<int> clean_ints;
    std::istringstream iss(buffer);
    std::string input_line;
    while (std::getline(iss, input_line)) {
        std::string trimmed = input_line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        if (trimmed.empty() || trimmed[0] == '#') continue;
        std::istringstream linestream(trimmed);
        int value;
        if (linestream >> value) {
            clean_ints.push_back(value);
            if (clean_ints.size() >= MAX_INTS) break;
        }
    }
    int n = clean_ints.size();
    if (n == 0) {
        std::cerr << "No valid integers found in input.txt\n";
        return 1;
    }
    if (!is_power_of_2(n)) {
        std::cerr << "Input size (" << n << ") is not a power of 2. FFT requires N=2^k.\n";
        return 1;
    }

    // Copy to array for mmap/VirtualAlloc
    int input_data[MAX_INTS];
    for (int i = 0; i < n; ++i) input_data[i] = clean_ints[i];

    // Linux: mmap() -> Windows: VirtualAlloc()
    std::cout << "[INFO] Memory mapping input data (VirtualAlloc)\n";
    int* mapped = (int*)VirtualAlloc(NULL, sizeof(int) * n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mapped) {
        print_win_error("Error with VirtualAlloc");
        return 1;
    }
    memcpy(mapped, input_data, sizeof(int) * n);

    // --- Timing: Start FFT timer ---
    LONGLONG fft_start = qpc_now();

    // FFT on full input
    std::vector<std::complex<double>> data_c(n);
    for (int i = 0; i < n; ++i) data_c[i] = std::complex<double>(mapped[i], 0.0);
    fft(data_c);

    // --- Timing: End FFT timer ---
    LONGLONG fft_end = qpc_now();
    double fft_time = qpc_seconds(fft_start, fft_end);
    std::cout << "[INFO] FFT Time: " << fft_time << " seconds\n";

    // Prepare output buffer: real and imag as doubles
    std::vector<double> result_buf(2 * n);
    for (int i = 0; i < n; ++i) {
        result_buf[2 * i] = data_c[i].real();
        result_buf[2 * i + 1] = data_c[i].imag();
    }

    // Linux: shmget()/shmat() -> Windows: CreateFileMapping()/MapViewOfFile()
    std::cout << "[INFO] Creating shared memory (CreateFileMapping)\n";
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(double) * 2 * n, NULL);
    if (!hMap) {
        print_win_error("Error with CreateFileMapping");
        VirtualFree(mapped, 0, MEM_RELEASE);
        return 1;
    }
    double* shm_ptr = (double*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(double) * 2 * n);
    if (!shm_ptr) {
        print_win_error("Error with MapViewOfFile");
        VirtualFree(mapped, 0, MEM_RELEASE);
        CloseHandle(hMap);
        return 1;
    }
    memcpy(shm_ptr, result_buf.data(), sizeof(double) * 2 * n);

    // Linux: open() -> Windows: CreateFile()
    std::cout << "[INFO] Opening output file (CreateFile)\n";
    HANDLE hOut = CreateFileA(OUTPUT_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        print_win_error("Error opening output.txt");
        VirtualFree(mapped, 0, MEM_RELEASE);
        UnmapViewOfFile(shm_ptr);
        CloseHandle(hMap);
        return 1;
    }
    // Linux: write() -> Windows: WriteFile()
    std::cout << "[INFO] Writing FFT output to file (WriteFile)\n";
    std::string header = "Index    Real       Imag\n";
    DWORD written = 0;
    if (!WriteFile(hOut, header.c_str(), header.size(), &written, NULL)) {
        print_win_error("Error writing output.txt");
        CloseHandle(hOut);
        VirtualFree(mapped, 0, MEM_RELEASE);
        UnmapViewOfFile(shm_ptr);
        CloseHandle(hMap);
        return 1;
    }
    char line[128];
    for (int i = 0; i < n; ++i) {
        int len = snprintf(line, sizeof(line), "%d    %9.3f   %9.3f\n", i, shm_ptr[2 * i], shm_ptr[2 * i + 1]);
        if (!WriteFile(hOut, line, len, &written, NULL)) {
            print_win_error("Error writing output.txt");
            CloseHandle(hOut);
            VirtualFree(mapped, 0, MEM_RELEASE);
            UnmapViewOfFile(shm_ptr);
            CloseHandle(hMap);
            return 1;
        }
    }
    std::cout << "[INFO] Closing output file (CloseHandle)\n";
    CloseHandle(hOut);

    // Linux: chmod(), chown() -> Windows: SetFileSecurity() (simulated)
    std::cout << "[INFO] (Simulated) Changing permissions/ownership (SetFileSecurity)\n";

    // Linux: munmap() -> Windows: VirtualFree()
    std::cout << "[INFO] Unmapping memory (VirtualFree)\n";
    VirtualFree(mapped, 0, MEM_RELEASE);
    UnmapViewOfFile(shm_ptr);
    CloseHandle(hMap);

    // --- Timing: End total execution timer ---
    LONGLONG total_end = qpc_now();
    double total_time = qpc_seconds(total_start, total_end);
    std::cout << "[INFO] Total Execution Time: " << total_time << " seconds\n";
    std::cout << "[INFO] Done!\n";
    return 0;
}
