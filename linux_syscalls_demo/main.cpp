#include <ctime> // For timing
/*
-------------------------------------------------------------
## | Linux        | Windows API              | Purpose         |
| open()       | CreateFile()             | File open       |
| read()       | ReadFile()               | File read       |
| write()      | WriteFile()              | File write      |
| close()      | CloseHandle()            | Close file      |
| fork()       | CreateProcess()          | Process create  |
| exec()       | CreateProcess()          | Execute program |
| wait()       | WaitForSingleObject()    | Wait process    |
| mmap()       | VirtualAlloc()           | Memory mapping  |
| munmap()     | VirtualFree()            | Free memory     |
| pipe()       | CreatePipe()             | IPC pipe        |
| shmget()     | CreateFileMapping()      | Shared memory   |
| shmat()      | MapViewOfFile()          | Attach memory   |
| chmod()      | SetFileSecurity()        | Permissions     |
| chown()      | SetFileSecurity()        | Ownership       |
| umask()      | (No direct equivalent)   | Default perms   |
-------------------------------------------------------------

Simple Linux System Calls Demo Project
- Demonstrates 15 system calls in a clear, beginner-friendly way
- Compile: g++ main.cpp -o project
- Run: ./project
*/

#include <iostream>
#include <fcntl.h>      // open
#include <unistd.h>     // read, write, close, fork, pipe, exec, chown
#include <sys/wait.h>   // wait
#include <sys/mman.h>   // mmap, munmap
#include <sys/types.h>  // shmget, shmat, chown
#include <sys/ipc.h>    // shmget
#include <sys/shm.h>    // shmget, shmat
#include <sys/stat.h>   // chmod
#include <pwd.h>        // getpwnam
#include <cstring>      // strerror, memcpy
#include <errno.h>
#include <vector>
#include <sstream>
#include <complex> // For FFT
#include <cmath>

#define INPUT_FILE "input.txt"
#define OUTPUT_FILE "output.txt"
#define SHM_KEY 1234
#define MAX_INTS 100


// --- FFT Implementation (Cooley-Tukey, in-place, recursive) ---
// Input: vector of complex numbers, size N (must be power of 2)
// Output: in-place FFT result
void fft(std::vector<std::complex<double>>& a) {
    int n = a.size();
    if (n <= 1) return;
    // Divide
    std::vector<std::complex<double>> even(n / 2), odd(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        even[i] = a[i * 2];
        odd[i] = a[i * 2 + 1];
    }
    // Conquer
    fft(even);
    fft(odd);
    // Combine
    for (int k = 0; k < n / 2; ++k) {
        std::complex<double> t = std::polar(1.0, -2 * M_PI * k / n) * odd[k];
        a[k] = even[k] + t;
        a[k + n / 2] = even[k] - t;
    }
}

// Utility: Check if n is a power of 2
bool is_power_of_2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int main() {

    // --- Timing: Start total execution timer ---
    clock_t total_start = clock();

    // System call #14: umask()
    std::cout << "[INFO] Setting umask (umask)\n";
    umask(0022); // Set default permissions


    // --- Timing: Start file read timer ---
    clock_t read_start = clock();

    // System call #1: open()
    std::cout << "[INFO] Opening input file (open)\n";
    int fd = open(INPUT_FILE, O_RDONLY);
    if (fd < 0) {
        std::cerr << "Error opening input.txt: " << strerror(errno) << "\n";
        return 1;
    }

    // System call #2: read()
    std::cout << "[INFO] Reading input file (read)\n";
    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        std::cerr << "Error reading input.txt: " << strerror(errno) << "\n";
        close(fd);
        return 1;
    }
    buffer[bytes_read] = '\0'; // Null-terminate

    // System call #4: close()
    std::cout << "[INFO] Closing input file (close)\n";
    close(fd);

    // --- Timing: End file read timer ---
    clock_t read_end = clock();
    double file_read_time = (double)(read_end - read_start) / CLOCKS_PER_SEC;
    std::cout << "[INFO] File Read Time: " << file_read_time << " seconds\n";

    // Parse buffer line-by-line, extract only integers, skip comments
    std::vector<int> clean_ints;
    std::istringstream iss(buffer);
    std::string input_line;
    while (std::getline(iss, input_line)) {
        // Skip lines starting with '#'
        std::string trimmed = input_line;
        // Remove leading spaces
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        if (trimmed.empty() || trimmed[0] == '#') continue;
        std::istringstream linestream(trimmed);
        int value;
        // Extract integer if present at start of line
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

    // Copy to array for mmap and IPC
    int input_data[MAX_INTS];
    for (int i = 0; i < n; ++i) input_data[i] = clean_ints[i];


    // System call #8: mmap()
    std::cout << "[INFO] Memory mapping input data (mmap)\n";
    int* mapped = (int*)mmap(NULL, sizeof(int) * n, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mapped == MAP_FAILED) {
        std::cerr << "Error with mmap: " << strerror(errno) << "\n";
        return 1;
    }
    memcpy(mapped, input_data, sizeof(int) * n);

    // --- Timing: Start FFT timer ---
    clock_t fft_start = clock();

    // --- Perform FFT on full input (single process, correct) ---
    std::vector<std::complex<double>> data_c(n);
    for (int i = 0; i < n; ++i) data_c[i] = std::complex<double>(mapped[i], 0.0);
    fft(data_c);

    // --- Timing: End FFT timer ---
    clock_t fft_end = clock();
    double fft_time = (double)(fft_end - fft_start) / CLOCKS_PER_SEC;
    std::cout << "[INFO] FFT Time: " << fft_time << " seconds\n";

    // Prepare output buffer: real and imag as doubles
    std::vector<double> result_buf(2 * n);
    for (int i = 0; i < n; ++i) {
        result_buf[2 * i] = data_c[i].real();
        result_buf[2 * i + 1] = data_c[i].imag();
    }

    // System call #11: shmget()
    std::cout << "[INFO] Creating shared memory (shmget)\n";
    int shmid = shmget(SHM_KEY, sizeof(double) * 2 * n, IPC_CREAT | 0666);
    if (shmid < 0) {
        std::cerr << "Error with shmget: " << strerror(errno) << "\n";
        munmap(mapped, sizeof(int) * n);
        return 1;
    }
    // System call #12: shmat()
    std::cout << "[INFO] Attaching shared memory (shmat)\n";
    double* shm_ptr = (double*)shmat(shmid, NULL, 0);
    if (shm_ptr == (void*)-1) {
        std::cerr << "Error with shmat: " << strerror(errno) << "\n";
        munmap(mapped, sizeof(int) * n);
        return 1;
    }
    memcpy(shm_ptr, result_buf.data(), sizeof(double) * 2 * n);

    // System call #1: open()
    std::cout << "[INFO] Opening output file (open)\n";
    int outfd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) {
        std::cerr << "Error opening output.txt: " << strerror(errno) << "\n";
        munmap(mapped, sizeof(int) * n);
        return 1;
    }
    // System call #3: write()
    std::cout << "[INFO] Writing FFT output to file (write)\n";
    std::string header = "Index    Real       Imag\n";
    if (write(outfd, header.c_str(), header.size()) < 0) {
        std::cerr << "Error writing output.txt: " << strerror(errno) << "\n";
        close(outfd);
        munmap(mapped, sizeof(int) * n);
        return 1;
    }
    char line[128];
    for (int i = 0; i < n; ++i) {
        snprintf(line, sizeof(line), "%d    %9.3f   %9.3f\n", i, shm_ptr[2 * i], shm_ptr[2 * i + 1]);
        if (write(outfd, line, strlen(line)) < 0) {
            std::cerr << "Error writing output.txt: " << strerror(errno) << "\n";
            close(outfd);
            munmap(mapped, sizeof(int) * n);
            return 1;
        }
    }
    std::cout << "[INFO] Closing output file (close)\n";
    close(outfd);

    // System call #13: chmod()
    std::cout << "[INFO] Changing permissions (chmod)\n";
    chmod(OUTPUT_FILE, 0600); // Owner read/write only
    // System call #15: chown()
    std::cout << "[INFO] Changing ownership (chown)\n";
    struct passwd* pw = getpwnam("nobody");
    if (pw) chown(OUTPUT_FILE, pw->pw_uid, pw->pw_gid);
    else std::cout << "[WARN] User 'nobody' not found, skipping chown\n";
    std::cout << "[INFO] Unmapping memory (munmap)\n";
    munmap(mapped, sizeof(int) * n);

    // --- Timing: End total execution timer ---
    clock_t total_end = clock();
    double total_time = (double)(total_end - total_start) / CLOCKS_PER_SEC;
    std::cout << "[INFO] Total Execution Time: " << total_time << " seconds\n";
    std::cout << "[INFO] Done!\n";
    return 0;
}