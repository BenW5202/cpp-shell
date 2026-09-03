# C++ POSIX Shell

A custom, lightweight Unix shell written in C++17. This project implements core shell functionalities from scratch, interacting directly with the Linux operating system via POSIX system calls.

## Features
* **Command Execution:** Parses and executes system binaries by dynamically resolving the `$PATH` environment variable.
* **Process Management:** Utilizes `fork()`, `execv()`, and `waitpid()` for child process creation and lifecycle management.
* **Piping & Redirection:** Supports multi-stage pipelines (`|`) and standard I/O redirection (`>`, `>>`, `<`) using file descriptor manipulation (`pipe()`, `dup2()`).
* **Logical Operators:** Supports conditional execution using `&&` and `||`.
* **Background Processes:** Supports running processes in the background using `&`, with asynchronous zombie process reaping via a `SIGCHLD` signal handler.
* **Custom Built-ins:** Includes native implementations of `cd`, `echo`, `type`, `history`, and `exit`.

## Building and Running

### Prerequisites
* CMake (3.10 or higher)
* A C++17 compatible compiler (GCC/Clang)
* A POSIX-compliant OS (Linux/macOS)

### Standard Linux/macOS Build
```bash
mkdir build
cd build
cmake ..
make
./cpp-shell
```

### Developing on Windows (VS Code + WSL)
This project relies on POSIX system calls (like `fork` and `execv`), which are not natively available on Windows. If you are on Windows, you must use **WSL (Windows Subsystem for Linux)**.

1. Ensure you have [WSL installed](https://learn.microsoft.com/en-us/windows/wsl/install) with a Linux distribution (e.g., Ubuntu).
2. Install [Visual Studio Code](https://code.visualstudio.com/) and the **WSL Extension**.
3. Open your WSL terminal, clone the repository, and open it in VS Code:
   ```bash
   git clone https://github.com/YourUsername/cpp-shell.git
   cd cpp-shell
   code .
   ```
4. Install the **C/C++** and **CMake Tools** extensions in VS Code.
5. Let CMake Tools configure the project, then click **Build** and **Run** at the bottom of your VS Code window, or run the standard Linux build commands in the integrated VS Code terminal.
