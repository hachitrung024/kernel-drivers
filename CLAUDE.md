# kernel-drivers

## Project
Out-of-tree Linux kernel module development on Raspberry Pi 4.
Each subdirectory in drivers/ is an independent module.

## Session Start Protocol
1. Read TASKS.md — identify current task
2. Read PROGRESS.md — load context from previous session
3. Read relevant docs/<driver>.md if working on that driver
4. Confirm scope before writing any code

## Environment
- Host: edit, git, and build trigger
- Pi: build only via SSH
- Build: `ssh pi "cd ~/driver && make"`
- Kernel headers on Pi: /lib/modules/$(uname -r)/build

## Build Check
- Sync source to Pi: `rsync -av drivers/01-dht20/ pi:~/driver/`
- Build: `ssh pi "cd ~/driver && make"`
- Must be zero warnings before task is considered done

## Task Complete When
- Build has zero warnings
- Developer has read and understood the code
- Log appended to PROGRESS.md

## PROGRESS.md Log Template
### Task <ID> - <title>
- Date:
- Driver:
- What was done:
- Build result:
- Known issues:

## Conventions
- One driver per directory: drivers/<index>-<name>/
- English only: source, comments, all files
- Kernel coding style — checkpatch.pl clean
- docs/<driver>.md is the reference — consult before implementing