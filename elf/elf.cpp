// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#include <elf.hpp>

#include <fstream>
#include <iostream>

#include <elfio/elfio.hpp>

namespace
{

std::optional<std::vector<AxELFSymbol>> convert_symbols(const ELFIO::elfio& elf, ELFIO::section& section)
{
    const ELFIO::symbol_section_accessor symbols{elf, &section};

    std::vector<AxELFSymbol> output{};
    output.reserve(symbols.get_symbols_num());

    for(uint64_t i = 0; i < symbols.get_symbols_num(); ++i)
    {
        auto& current = output.emplace_back();
        if(symbols.get_symbol(i, current.name, current.value, current.size, current.binding, current.type,
               current.shndx, current.visibility))
        {
            std::cerr << "Warning: Failed to get ELF symbol name" << std::endl;
            return std::nullopt;
        }

        current.visibility &= 0x03;
    }

    return output;
}

std::optional<AxELFFile> convert_elf(const ELFIO::elfio& elf)
{
    // auto&& header = elf.header;

    AxELFFile output;
    output.sections.reserve(elf.sections.size());

    for(auto&& section : elf.sections)
    {
        if(section->get_type() == AX_SHT_SYMTAB) // preprocess symbol table for later uses!
        {
            auto symbols = convert_symbols(elf, *section);
            if(symbols)
            {
                output.symbols = *std::move(symbols);
            }
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
        if((current.flags & AX_SHF_ALLOC) != 0)
        {
            const char* content = section->get_data();
            if(!content)
            {
                std::cerr << "Error: Failed to get ELF section content" << std::endl;
                return std::nullopt;
            }

            current.content.resize(current.size);
            std::copy_n(content, current.size, current.content.data());
        }
    }

    return output;
}

}

std::optional<AxELFFile> AxELFFile::from_file(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios_base::binary};
    if(!file.is_open())
    {
        std::cerr << "Failed to open file \"" << path.generic_string() << "\"" << std::endl;
        return std::nullopt;
    }

    ELFIO::elfio elf{};
    if(!elf.load(file))
    {
        std::cerr << "Invalid parse ELF file \"" << path.generic_string() << "\"" << std::endl;
        return std::nullopt;
    }

    if(elf.get_class() != ELFIO::ELFCLASS64 || elf.get_encoding() != ELFIO::ELFDATA2LSB)
    {
      std::cerr << "ELF file \"" << path.generic_string() << "\" is not a LE64 ELF" << std::endl;
      return std::nullopt;
    }

    return convert_elf(elf);
}
