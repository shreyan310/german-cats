# Workstation Monitor

A Windows dashboard that uses a webcam, keyboard and mouse to estimate posture strain, eye strain, repetitive activity and prolonged work.

## Install and run on Windows (64-bit)

1. Download and install MSYS2 from https://www.msys2.org/. Open **MSYS2 UCRT64** from the Start menu.

2. Update MSYS2. If it asks you to close the terminal, reopen **MSYS2 UCRT64** and run the command again.

   ```bash
   pacman -Syu
   ```

3. Install the compiler, Git, OpenCV and the other dependencies. Press Enter when prompted to proceed.

   ```bash
   pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-opencv mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-qt6-base
   ```

4. Download the project and enter its folder.

   ```bash
   git clone https://github.com/shreyan310/german-cats.git
   cd german-cats
   ```

5. Check that the first command prints an OpenCV 5.x version. The second command builds and opens the dashboard.

   ```bash
   pkg-config --modversion opencv5
   bash test-now.sh
   ```

6. Choose **Relax** or **Focus**. Sit upright with your shoulders visible and face the camera with your eyes open. Calibration starts automatically. To recalibrate, click the dashboard and press **C** for posture or **B** for eyes.

Use the computer normally for at least a minute while the readings settle. A numeric zero is a valid score. The app creates `UTEK2026/data/metrics.json` automatically.

## Make a portable Windows copy

Close the dashboard. From the project folder in MSYS2 UCRT64, run:

```bash
bash build.sh
```

When the build finishes, open `build/WorkstationMonitor-Windows.zip`. Extract the entire ZIP to a writable folder and double-click **Workstation.exe**. Keep the DLLs, models and `monitor.exe` together in that folder.

## Credits

Emre contributed camera tracking. The keyboard/mouse teammate contributed input tracking. Shreyan integrated the trackers, scoring, alerts and dashboard with AI assistance. The project uses OpenCV and PicoJSON.
