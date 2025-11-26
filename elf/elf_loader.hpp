// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#ifndef AXELFLOADER_HPP_INCLUDED
#define AXELFLOADER_HPP_INCLUDED

#include "elf.hpp"

#include <span>

class AxCore;

// load an ELF file and put PC at specified entry point location
// This function panics on error!
void ax_load_elf_program(AxCore& core, const std::filesystem::path& path, std::string_view entry_point_name);
void ax_load_elf_program(AxCore& core, const void* buffer, size_t buffer_size, std::string_view entry_point_name);

// load an ELF file
// This function panics on error!
// Entry point is always main, and main always receive the argc and argv arguments
// r0 (=sp) is initialized at 0x801FFFF8 leaving 1Mio of stack size
// Argv values are stack allocated and given in a0 and a1
// This function adds code to emulate an hosted environment (i.e. can receive arguments):
// ```altairx-asm
// _entry:
//    ; argc and argv are initialized by the VM by writting to memory and registers
//    ; at this point:
//    ; - r0 == stack begin
//    ; - r1 == argc
//    ; - r2 == argv
//    call @main ; init LR too to come back here after main returns!
//    moveix @main
//    nop
//    add.d r2, r1, 0 ; exit code, returned by main
//    movei r1, 1 ; exit syscall
//    syscall
//    nop
// ```
void ax_load_elf_hosted_program(AxCore& core, const std::filesystem::path& path, std::span<const std::string_view> argv);
void ax_load_elf_hosted_program(AxCore& core, const void* buffer, size_t buffer_size, std::string_view program_name, std::span<const std::string_view> argv);

#endif
