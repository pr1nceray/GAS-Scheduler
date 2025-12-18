# Game Daemon & Wrapper Setup

This guide details the process for setting up the VM environment, compiling the source code, and launching the game daemon and instances.

## Prerequisites

* **OS:** Standard Ubuntu Environment
* **Compiler:** A C++20 compliant compiler (Tested with `g++13`)
* **VNC Client:** TigerVNC (Installed on your local machine)

---

## 1. VM Environment Setup

1.  **Connect to your Ubuntu VM.**
2.  **Install System Dependencies:**
    Run the helper script to install prerequisites (note: this does not install the compiler, or nvidia drivers).
    ```bash
    chmod -R 777 ./setup/vm_installer.sh
    sudo ./setup/vm_installer.sh
    ```
3.  **Verify Compiler:**
    Ensure you have a compiler installed that supports C++20.
    ```bash
    g++ --version
    # Ensure version is 13 or higher (or equivalent for C++20 support)
    ```
4.  **Start Headless Screen:**
    Execute the specific commands found inside `./setup/vm_helper.txt` to start the headless screen instance. do not execute them in a bash file, just run them in a console. note some commands (xorg, startxfce4) launch in the background.

---

## 2. Remote Desktop Connection

To view the desktop environment, you must tunnel through SSH and connect via VNC.

1.  **Disconnect** from your current VM session.
2.  **Reconnect with Port Forwarding:**
    Use the `-L` flag to forward port 5901. Replace the key path, user, and IP with your credentials.
    ```bash
    ssh -L 5901:localhost:5901 -i /path/to/key user@ip_address
    ```
3.  **Launch VNC Viewer Locally:**
    Open **TigerVNC** on your local machine.
4.  **Connect:**
    When prompted for the VNC server address, enter:
    `localhost:5901`

You should now see the remote desktop environment.

---

## 3. Installation & Building

### Clone the Repository
Clone the repository to the VM.
> **Important:** This repository uses **Git Large File Storage (LFS)**. Ensure Git LFS is installed and initialized so game assets download correctly.

### Build the Wrapper
1.  Navigate to the wrapper directory:
    ```bash
    cd src/wrapper
    ```
    
2.  Compile the code:
    ```bash
    make clean && make
    ```
    *Note: You must modify the makefile to point to the right include directories since we link agaisnt nvml.h. i have provided the makefiles we use in setup/daemeon_vm_makefile and setup/wrapper_vm_makefile. The important parts in these files is the -I and -L flags. In addition, You may see warnings regarding unused variables or function values. You can ignore these; they occur because debug options are currently disabled.*

### Build the Daemon
1.  Navigate to the daemon directory:
    ```bash
    cd ../daemon
    ```
2.  Compile the code:
    ```bash
    make clean && make
    ```
    *Again, please ignore compilation warnings.*

---

## 4. Configuration & Launch

### Start the Daemon
1.  Open a terminal.
2.  From the `src/daemon` directory, launch the daemon executable:
    ```bash
    ./daemon.out
    ```

### Launch Multiple Instances (urp_demo)
1.  **Prepare the Directory:**
    Open a **new** terminal window and navigate to the game directory:
    ```bash
    cd ./games/urp_demo
    ```
2.  **Setup the Launcher:**
    Copy the helper script into the game folder:
    ```bash
    cp ../../setup/helper_launcher.sh .
    ```
3.  **Configure the Library Path:**
    Open `helper_launcher.sh` in a text editor. Locate the variable for the shared library and set it to the path of the `libhook.so` file (generated in `src/wrapper` during the build step).
4.  **Set Permissions:**
    Ensure both the launcher script and the game executable have execution permissions.
    ```bash
    chmod -R 777 helper_launcher.sh
    chmod -R 777 built.x86_64  # Replace 'built.x86_64' with the game name; urp demo is built.x86_64
    ```
5.  **Run the Game:**
    Execute the launcher. This will automatically spawn 4 instances of the game.
    ```bash
    ./helper_launcher.sh
    ```

### Alternative: Launch Single Instance
If you wish to launch the wrapper with a single game instance manually, use the `LD_PRELOAD` command:

```bash
LD_PRELOAD=/path/to/src/wrapper/libhook.so ./game_executable
```
ensure the daemon is running when you launch, or else it will not launch.

### Note: closing daemon will close all tracked processes
Daemon Lifecycle

    Warning: Closing the daemon (./daemon.out) will immediately terminate all tracked game processes. Do not close the daemon terminal unless you intend to stop all active game sessions.

Headless Display Issues

If your headless instance becomes unresponsive, buggy, or fails to connect:

    Clean Up Processes: Ensure all processes involving the desktop setup (X11, Xorg, Xfce4) are killed to prevent conflicts.

    Restart: Once the processes are cleared, re-run the startup commands located in ./setup/vm_helper.txt.