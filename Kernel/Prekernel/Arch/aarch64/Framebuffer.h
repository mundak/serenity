/*
 * Copyright (c) 2021, Marcin Undak <mcinek@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Prekernel {

class Framebuffer {
public:
    static Framebuffer& the();

    bool initialized() const { return m_initialized; }
    u16 width() const { return m_width; }
    u16 height() const { return m_height; }
    u8 depth() const { return m_depth; }
    u8* buffer() const { return m_buffer; }
    u32 buffer_size() const { return m_buffer_size; }
    u32 pitch() const { return m_pitch; }

private:
    u16 m_width;
    u16 m_height;
    u8 m_depth;
    u8* m_buffer;
    u32 m_buffer_size;
    u32 m_pitch;
    bool m_initialized;

    Framebuffer();
};
}
