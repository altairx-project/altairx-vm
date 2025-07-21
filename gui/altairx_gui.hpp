// Copyright (c) Kannagi, Alexy Pellegrini
// MIT License, see LICENSE for details

#ifndef AXDEBUGGERGUI_HPP_INCLUDED
#define AXDEBUGGERGUI_HPP_INCLUDED

#include <memory>

class AltairXGUI
{
    struct Impl;
    struct Data;

public:
    AltairXGUI();
    ~AltairXGUI();
    AltairXGUI(const AltairXGUI&) = delete;
    AltairXGUI& operator=(const AltairXGUI&) = delete;
    AltairXGUI(AltairXGUI&&) noexcept = delete;
    AltairXGUI& operator=(AltairXGUI&&) noexcept = delete;

    int run();

private:
    void draw_ui();

private:
    std::unique_ptr<Impl> m_impl;
    std::unique_ptr<Data> m_data;
};

#endif
