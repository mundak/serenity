/*
 * Copyright (c) 2021, Marcin Undak <mcinek@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Prekernel/Arch/aarch64/Framebuffer.h>
#include <Kernel/Prekernel/Arch/aarch64/Mailbox.h>
#include <Kernel/Prekernel/Arch/aarch64/Utils.h>

namespace Prekernel {

Framebuffer::Framebuffer()
{
    // FIXME: query HDMI for best mode
    // https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/tvservice/tvservice.c
    
    m_width = 1280;
    m_height = 720;
    m_depth = 32;
    m_initialized = Mailbox::init_framebuffer(m_width, m_height, m_depth, &m_buffer, &m_buffer_size, &m_pitch);

    if (!m_initialized) {
        warnln("Failed to initialize framebuffer.");
        return;
    }

    dbgln("Initialized framebuffer: 1280 x 720 @ 32 bits");
}

Framebuffer& Framebuffer::the()
{
    static Framebuffer instance;
    return instance;
}

}
