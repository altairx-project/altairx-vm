// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#ifndef ALTAIRX_HPP_INCLUDED
#define ALTAIRX_HPP_INCLUDED

#include <cstdint>
#include <array>
#include <filesystem>

#include <memory.hpp>
#include <core.hpp>

enum class AxExecutionMode
{
    DEFAULT = 0,
    DEBUG = 1
};

class AltairX
{
public:
    AltairX(size_t nwram, size_t nspmt, size_t nspm2);

    // load an ELF file and put PC at specified entry point location
    void load_program(const std::filesystem::path& path, std::string_view entry_point_name);

    // load an ELF file
    // See ax_load_elf_hosted_program
    void load_hosted_program(const std::filesystem::path& path, std::span<const std::string_view> argv);

    // tbd
    void load_kernel(const std::filesystem::path& path);

    int run(AxExecutionMode mode);

private:
    AxMemory m_memory;
    AxCore m_core;
};

#endif
