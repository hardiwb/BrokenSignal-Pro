# PlatformIO Safety Rules

## PlatformIO / CMake Safety Rule

When a PlatformIO build fails, especially with CMake-related errors, do NOT assume the global PlatformIO installation is corrupted.

Do NOT delete, reinstall, prune, upgrade, downgrade, or modify:

- `%USERPROFILE%\.platformio`
- `%USERPROFILE%\.platformio\packages`
- `%USERPROFILE%\.platformio\platforms`
- PlatformIO's Python environment
- global ESP-IDF / Arduino framework packages
- global toolchains

unless I explicitly approve it.

When switching projects or environments, use this troubleshooting order:

1. Run the normal project build first:
   `pio run -e <env>`

2. If it is a CMake/build-state error, inspect the FIRST meaningful error in the log.

3. If stale build state is plausible, remove ONLY:
   `.pio/build/`

   then rebuild.

4. If the error points to broken/mismatched project libraries, remove ONLY:
   `.pio/libdeps/<env>/`

   then rebuild.

5. If project-local state still appears corrupted, you may remove the entire project's:
   `.pio/`

   then rebuild.

6. After that, inspect:
   - `platformio.ini`
   - `CMakeLists.txt`
   - component `CMakeLists.txt`
   - `lib_deps`
   - expected PlatformIO platform version
   - expected ESP-IDF / Arduino framework version

7. Different projects may legitimately use different framework/toolchain versions. Multiple versions can coexist in the global PlatformIO package directory. Do not delete an existing version just because another project needs a different one.

8. A CMake failure, compiler error, linker error, missing header, or dependency error is NOT sufficient evidence that the global PlatformIO installation is corrupted.

9. If you believe a global PlatformIO package is genuinely corrupted, STOP before changing anything.

Report:
- exact package name
- installed version
- exact error
- evidence that the global package itself is corrupted
- proposed repair command
- what would need to be downloaded again

Wait for my explicit approval before performing the repair.

Primary rule:

PROJECT-LOCAL CLEANUP FIRST.
GLOBAL PLATFORMIO REPAIR LAST.
NEVER DELETE `.platformio` JUST TO GET A CLEAN BUILD.

## Important

The current PlatformIO installation is assumed to be working unless there is strong evidence otherwise.

Do NOT repair, reinstall, upgrade, downgrade, prune, or delete the global PlatformIO environment automatically.

NEVER delete or modify:

- `%USERPROFILE%\.platformio`
- `%USERPROFILE%\.platformio\packages`
- `%USERPROFILE%\.platformio\platforms`
- PlatformIO's Python environment

Do NOT run these commands unless I explicitly ask:

- `pio upgrade`
- `pio pkg update`
- `pio system prune`
- forced package reinstall
- PlatformIO Core reinstall
- framework/toolchain reinstall

## When switching projects

First try:

`pio run`

A different project may legitimately use another framework or platform version. This does NOT mean the existing PlatformIO installation is broken.

If PlatformIO needs a package that is not installed yet, allow PlatformIO to install that required package normally.

Do NOT remove existing global packages just because another project requires a different version.

Multiple framework/toolchain versions may coexist.

## If a build fails

Follow this order:

1. Read the complete build error.
2. Determine whether it is:
   - source-code error
   - library/dependency error
   - project configuration error
   - missing package
   - actual global PlatformIO corruption
3. Fix source code or `platformio.ini` first when appropriate.
4. Run `pio run` again.
5. If project build artifacts appear stale, run:

   `pio run -t clean`

6. If necessary, delete ONLY this project's `.pio` directory and rebuild.
7. Never delete the global `%USERPROFILE%\.platformio` directory as a troubleshooting step.
8. If the error genuinely appears to come from a corrupted global framework, compiler, toolchain, or PlatformIO package:
   STOP.

Report to me:

- the exact failing package
- its installed version
- the relevant error
- why you believe the global package is corrupted
- the proposed repair command

WAIT for explicit permission before modifying the global PlatformIO environment.

## Package/version policy

Do not change a working PlatformIO platform/framework version just to obtain the newest version.

Do not replace pinned versions with `stable`, `latest`, or another moving target.

Preserve known-working versions unless a project specifically requires a change.

## Primary rule

A compilation failure is NOT sufficient evidence that PlatformIO is corrupted.

Prefer repairing the project over repairing the development environment.