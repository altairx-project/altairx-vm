// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include "elf.hpp"
#include "panic.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <elfio/elf_types.hpp>
#include <elfio/elfio.hpp>
#include <elfio/elfio_section.hpp>
#include <elfio/elfio_symbols.hpp>

namespace
{

std::vector<AxELFSymbol> convert_symbols(const ELFIO::elfio& elf, ELFIO::section& section)
{
    const ELFIO::symbol_section_accessor symbols{elf, &section};

    std::vector<AxELFSymbol> output{};
    output.reserve(symbols.get_symbols_num());

    for(uint64_t i = 0; i < symbols.get_symbols_num(); ++i)
    {
        auto& current = output.emplace_back();
        if(!symbols.get_symbol(i, current.name, current.value, current.size, current.binding, current.type,
               current.shndx, current.visibility))
        {
            ax_panic("Failed to get ELF symbol name, symbol #", i);
        }

        current.visibility &= 0x03;
    }

    return output;
}

void convert_elf(const ELFIO::elfio& elf, AxELFFile& output)
{
    if(elf.get_class() != ELFIO::ELFCLASS64 || elf.get_encoding() != ELFIO::ELFDATA2LSB)
    {
        ax_panic("ELF file is not a LE64 ELF.");
    }

    output.sections.reserve(elf.sections.size());

    for(auto&& section : elf.sections)
    {
        if(section->get_type() == AX_SHT_SYMTAB) // preprocess symbol table for later uses!
        {
            auto symbols = convert_symbols(elf, *section);
            output.symbols.insert(std::begin(output.symbols),
                std::move_iterator{std::begin(symbols)},
                std::move_iterator{std::end(symbols)});
        }

        // store raw section info
        auto& current = output.sections.emplace_back();
        current.type = section->get_type();
        current.flags = section->get_flags();
        current.addr = section->get_address();
        current.offset = section->get_offset();
        current.size = section->get_size();
        current.link = section->get_link();
        current.info = section->get_info();
        current.addralign = section->get_addr_align();
        current.entsize = section->get_entry_size();

        // get content of allocatable sections
        // NOBITS sections have no registered content, loader need take it into account.
        if((current.flags & AX_SHF_ALLOC) != 0 &&
            current.type != AX_SHT_NOBITS &&
            current.size != 0)
        {
            current.content.resize(current.size);
            const char* content = section->get_data();
            if(!content)
            {
                ax_panic("Error: Failed to get ELF section content: section \"", section->get_name(), '\"');
            }

            std::copy_n(content, current.size, current.content.data());
        }
    }
}

}

AxELFFile::AxELFFile(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios_base::binary};
    if(!file.is_open())
    {
        ax_panic("Failed to open file \"", path.generic_string(), "\"");
    }

    ELFIO::elfio elf{};
    if(!elf.load(file, true))
    {
        ax_panic("Invalid ELF file \"", path.generic_string(), "\"");
    }

    convert_elf(elf, *this);
}

AxELFFile::AxELFFile(const void* buffer, size_t buffer_size)
{
    std::istringstream iss{
        std::string{static_cast<const char*>(buffer), buffer_size}
    };

    ELFIO::elfio elf{};
    if(!elf.load(iss, true))
    {
        ax_panic("Failed to parse ELF file.");
    }

    convert_elf(elf, *this);
}
