# AltairX Virtual Machine

AltairX Virtual Machine (AltairX VM) is a lightweight virtual machine designed to emulate our VLIW CPU architecture AltairX K1.
This project aims to provide a reference implementation of a minimal yet extensible virtual machine tailored to AltairX, suitable for research, emulation, and toolchain development.

## Features

TBD!

## 🛠️ Build & Installation

#### Prerequisites
  - CMake (version 3.21+ recommended)
  - A C++17 compatible compiler:
    - GCC ≥ 10 (recommended on Linux)
    - Clang ≥ 11
    - Clang-cl (recommended on Windows)
    - MSVC 2022

#### Building

On Windows use a "Developer PowerShell" or "Developer Command Prompt"!

```sh
git clone https://github.com/altairx-project/altairx-vm.git
cmake -S altairx-vm -B altairx-vm-build
cmake --build altairx-vm-build
cmake --install altairx-vm-build --prefix altairx-vm-install  # optional
```

#### Configuration options

| Option name           | Description                                                            | Default |
| --------------------- | ---------------------------------------------------------------------- | ------- |
| BUILD_TESTING         | Build unit tests.                                                      | OFF     |
| AltairXVM_USE_LTO     | Enable LTO if supported. This is recommended for release build.        | ON      |
| AltairXVM_ELF_SUPPORT | Enable ELF loading.                                                    | ON      |
| AltairXVM_BUILD_GUI   | Enable interactive GUI for the VM. This feature requires SDL3 library. | OFF     |

## 🔗 Dependencies

AltairX VM has multiple open-source dependencies. Their work makes AltairX VM possible!

Externally provided dependencies can be installed via package managers like vcpkg, conan, apt, pacman.
Other dependencies are internally built depending on configuration options.

The following dependencies are used optionally and detected automatically if available:
| Library | Homepage                               | Externally provided |
| ------- | -------------------------------------- | ------------------- |
| SDL     | https://github.com/libsdl-org/SDL      | Yes                 |
| ELFIO   | https://github.com/serge1/ELFIO.git    | No                  |
| ImGUI   | https://github.com/ocornut/imgui       | No                  |
| Catch   | https://github.com/catchorg/Catch2.git | No                  |
| libfmt  | https://github.com/fmtlib/fmt          | No                  |

## 📄 License

This project is licensed under the MIT License. See the LICENSE file for details.

## 💬 Contributions

Contributions are welcome!

Feel free to open issues, submit pull requests, or suggest improvements.
