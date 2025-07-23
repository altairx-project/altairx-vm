// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "altairx.hpp"

#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>

#ifdef AX_HAS_ELF
    #include <elf_loader.hpp>
#endif

#include <make_opcode.hpp>
#include <panic.hpp>

AltairX::AltairX(size_t nwram, size_t nspmt, size_t nspm2)
    : m_memory{nwram, nspmt, nspm2}
    , m_core{m_memory}
{
}

void AltairX::load_kernel(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    if(!file.is_open())
    {
        std::cerr << "Error : Impossible open kernel" << std::endl;
        return;
    }

    file.seekg(0, std::ios::end);
    std::streampos filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    void* rom = m_memory.map(m_core, AxMemory::ROM_BEGIN);
    file.read(reinterpret_cast<char*>(rom), filesize);
}

void AltairX::load_program(const std::filesystem::path& path, std::string_view entry_point_name)
{
#ifdef AX_HAS_ELF
    if(ax_load_elf_program(m_core, path, entry_point_name))
    {
        return;
    }
#endif

    // load raw executable file
    std::ifstream file{path, std::ios::binary};
    if(!file.is_open())
    {
        ax_panic("Error : Impossible open file \"", path.string(), "\"");
    }

    file.seekg(0, std::ios::end);
    std::streampos filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    void* wram = m_memory.map(m_core, AxMemory::WRAM_BEGIN);
    file.read(reinterpret_cast<char*>(wram), filesize);
    m_core.registers().pc = 4;
}

void AltairX::load_hosted_program(const std::filesystem::path& path, const std::vector<std::string_view>& argv)
{
#ifdef AX_HAS_ELF
    if(!ax_load_elf_hosted_program(m_core, path, argv))
    {
        ax_panic("Could not read ELF file \"", path.string(), "\"");
    }
#else
    ax_panic("Host emulation requires a build with ELF enabled!");
#endif
}

int AltairX::run(AxExecutionMode mode)
{
    using clock = std::chrono::steady_clock;
    using seconds = std::chrono::duration<double>;

    static constexpr std::size_t threshold = 1024 * 1024;

    auto tp1 = clock::now();
    std::size_t counter = 0;
    std::size_t cycles = 0;
    while(m_core.error() == 0)
    {
        m_core.cycle();
        m_core.syscall();

        counter += 1;
        cycles += 1;
        if(counter > threshold) // only check each few cycles...
        {
            const auto tp2 = clock::now();
            const auto delta = std::chrono::duration_cast<seconds>(tp2 - tp1).count();
            if(delta > 1.0) // ...and display if more than one second elapsed...
            {
                double frequency = static_cast<double>(cycles) / delta;
                std::cout << "Frequence : " << frequency / 1'000'000.0 << "MHz\n"; // no flush

                tp1 = clock::now();
                cycles = 0;
            }

            counter = 0;
        }
    }

    return m_core.error();
}
