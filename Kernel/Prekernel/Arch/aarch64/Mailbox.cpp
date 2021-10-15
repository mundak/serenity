/*
 * Copyright (c) 2021, Nico Weber <thakis@chromium.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Assertions.h>
#include <Kernel/Prekernel/Arch/aarch64/MMIO.h>
#include <Kernel/Prekernel/Arch/aarch64/Mailbox.h>
#include <Kernel/Prekernel/Arch/aarch64/Utils.h>

namespace Prekernel {

// There's one mailbox at MBOX_BASE_OFFSET for reading responses from VideoCore, and one at MBOX_BASE_OFFSET + 0x20 for sending requests.
// Each has its own status word.

constexpr u32 MBOX_BASE_OFFSET = 0xB880;
constexpr u32 MBOX_0 = MBOX_BASE_OFFSET;
constexpr u32 MBOX_1 = MBOX_BASE_OFFSET + 0x20;

constexpr u32 MBOX_READ_DATA = MBOX_0;
constexpr u32 MBOX_READ_POLL = MBOX_0 + 0x10;
constexpr u32 MBOX_READ_SENDER = MBOX_0 + 0x14;
constexpr u32 MBOX_READ_STATUS = MBOX_0 + 0x18;
constexpr u32 MBOX_READ_CONFIG = MBOX_0 + 0x1C;

constexpr u32 MBOX_WRITE_DATA = MBOX_1;
constexpr u32 MBOX_WRITE_STATUS = MBOX_1 + 0x18;

constexpr u32 MBOX_RESPONSE_SUCCESS = 0x8000'0000;
constexpr u32 MBOX_RESPONSE_PARTIAL = 0x8000'0001;
constexpr u32 MBOX_REQUEST = 0;
constexpr u32 MBOX_FULL = 0x8000'0000;
constexpr u32 MBOX_EMPTY = 0x4000'0000;

constexpr int ARM_TO_VIDEOCORE_CHANNEL = 8;

constexpr u32 MBOX_TAG_GET_FIRMWARE_VERSION = 0x0000'0001;
constexpr u32 MBOX_TAG_SET_CLOCK_RATE = 0x0003'8002;

// Framebuffer operations
constexpr u32 MBOX_TAG_ALLOCATE_BUFFER = 0x40001;
constexpr u32 MBOX_TAG_RELEASE_BUFFER = 0x48001;
constexpr u32 MBOX_TAG_BLANK_SCREEN = 0x40002;
constexpr u32 MBOX_TAG_GET_PHYSICAL_SIZE = 0x40003;
constexpr u32 MBOX_TAG_TEST_PHYSICAL_SIZE = 0x44003;
constexpr u32 MBOX_TAG_SET_PHYSICAL_SIZE = 0x48003;
constexpr u32 MBOX_TAG_GET_VIRTUAL_SIZE = 0x40004;
constexpr u32 MBOX_TAG_TEST_VIRTUAL_SIZE = 0x44004;
constexpr u32 MBOX_TAG_SET_VIRTUAL_SIZE = 0x48004;
constexpr u32 MBOX_TAG_GET_DEPTH = 0x40005;
constexpr u32 MBOX_TAG_TEST_DEPTH = 0x44005;
constexpr u32 MBOX_TAG_SET_DEPTH = 0x48005;
constexpr u32 MBOX_TAG_GET_PIXEL_ORDER = 0x40006;
constexpr u32 MBOX_TAG_TEST_PIXEL_ORDER = 0x44006;
constexpr u32 MBOX_TAG_SET_PIXEL_ORDER = 0x48006;
constexpr u32 MBOX_TAG_GET_ALPHA_MODE = 0x40007;
constexpr u32 MBOX_TAG_TEST_ALPHA_MODE = 0x44007;
constexpr u32 MBOX_TAG_SET_ALPHA_MODE = 0x48007;
constexpr u32 MBOX_TAG_GET_PITCH = 0x40008;
constexpr u32 MBOX_TAG_GET_VIRTUAL_OFFSET = 0x40009;
constexpr u32 MBOX_TAG_TEST_VIRTUAL_OFFSET = 0x44009;
constexpr u32 MBOX_TAG_SET_VIRTUAL_OFFSET = 0x48009;
constexpr u32 MBOX_TAG_GET_OVERSCAN = 0x4000A;
constexpr u32 MBOX_TAG_TEST_OVERSCAN = 0x4400A;
constexpr u32 MBOX_TAG_SET_OVERSCAN = 0x4800A;
constexpr u32 MBOX_TAG_GET_PALETTE = 0x4000B;
constexpr u32 MBOX_TAG_TEST_PALETTE = 0x4400B;
constexpr u32 MBOX_TAG_SET_PALETTE = 0x4800B;

static void wait_until_we_can_write(MMIO& mmio)
{
    // Since nothing else writes to the mailbox, this wait is mostly cargo-culted.
    // Most baremetal tutorials on the internet query MBOX_READ_STATUS here, which I think is incorrect and only works because this wait really isn't needed.
    while (mmio.read(MBOX_WRITE_STATUS) & MBOX_FULL)
        ;
}

static void wait_for_reply(MMIO& mmio)
{
    while (mmio.read(MBOX_READ_STATUS) & MBOX_EMPTY)
        ;
}

bool Mailbox::call(u8 channel, u32 volatile* __attribute__((aligned(16))) message)
{
    auto& mmio = MMIO::the();

    // The mailbox interface has a FIFO for message deliverly in both directions.
    // Responses can be delivered out of order to requests, but we currently ever only send on request at once.
    // It'd be nice to have an async interface here where we send a message, then return immediately, and read the response when an interrupt arrives.
    // But for now, this is synchronous.

    wait_until_we_can_write(mmio);

    // The mailbox message is 32-bit based, so this assumes that message is in the first 4 GiB.
    u32 request = static_cast<u32>(reinterpret_cast<FlatPtr>(message) & ~0xF) | (channel & 0xF);
    mmio.write(MBOX_WRITE_DATA, request);

    for (;;) {
        wait_for_reply(mmio);

        u32 response = mmio.read(MBOX_READ_DATA);
        // We keep at most one message in flight and do synchronous communication, so response will always be == request for us.
        if (response == request)
            return message[1] == MBOX_RESPONSE_SUCCESS;
    }

    return true;
}

u32 Mailbox::query_firmware_version()
{
    // See https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface for data format.
    u32 __attribute__((aligned(16))) message[7];
    message[0] = sizeof(message);
    message[1] = MBOX_REQUEST;

    message[2] = MBOX_TAG_GET_FIRMWARE_VERSION;
    message[3] = 0; // Tag data size. MBOX_TAG_GET_FIRMWARE_VERSION needs no arguments.
    message[4] = MBOX_REQUEST;
    message[5] = 0; // Trailing zero for request, room for data in response.

    message[6] = 0; // Room for trailing zero in response.

    if (call(ARM_TO_VIDEOCORE_CHANNEL, message) && message[2] == MBOX_TAG_GET_FIRMWARE_VERSION)
        return message[5];

    return 0xffff'ffff;
}

u32 Mailbox::set_clock_rate(ClockID clock_id, u32 rate_hz, bool skip_setting_turbo)
{
    u32 __attribute__((aligned(16))) message[9];
    message[0] = sizeof(message);
    message[1] = MBOX_REQUEST;

    message[2] = MBOX_TAG_SET_CLOCK_RATE;
    message[3] = 12; // Tag data size.
    message[4] = MBOX_REQUEST;
    message[5] = static_cast<u32>(clock_id);
    message[6] = rate_hz;
    message[7] = skip_setting_turbo ? 1 : 0;

    message[8] = 0;

    call(ARM_TO_VIDEOCORE_CHANNEL, message);
    return message[6];
}

bool Mailbox::init_framebuffer(u16 width, u16 height, u8 depth, u8** buffer_ptr, u32* size, u32* pitch)
{
    VERIFY(width > 0);
    VERIFY(height > 0);
    VERIFY(depth > 0);
    VERIFY(buffer_ptr != nullptr);
    VERIFY(size != nullptr);
    VERIFY(pitch != nullptr);

    const u32 offset_x = 0;
    const u32 offset_y = 0;
    const u32 pixel_mode = 1 /* RGB */;
    const u32 buffer_alignment = 4096;

    u32 __attribute__((aligned(16))) message[36];
    message[0] = sizeof(message);
    message[1] = MBOX_REQUEST;

    message[2] = MBOX_TAG_SET_PHYSICAL_SIZE;
    message[3] = 8;
    message[4] = 8;
    message[5] = width;
    message[6] = height;

    message[7] = MBOX_TAG_SET_VIRTUAL_SIZE;
    message[8] = 8;
    message[9] = 8;
    message[10] = width;
    message[11] = height;

    message[12] = MBOX_TAG_SET_VIRTUAL_OFFSET;
    message[13] = 8;
    message[14] = 8;
    message[15] = offset_x;
    message[16] = offset_y;

    message[17] = MBOX_TAG_SET_DEPTH;
    message[18] = 4;
    message[19] = 4;
    message[20] = depth;

    message[21] = MBOX_TAG_SET_PIXEL_ORDER;
    message[22] = 4;
    message[23] = 4;
    message[24] = pixel_mode;

    message[25] = MBOX_TAG_ALLOCATE_BUFFER;
    message[26] = 8;
    message[27] = 8;
    message[28] = buffer_alignment;
    message[29] = 0;

    // FIXME: without this line QEMU freezes...
    dbgln("QEMU HACK! sleep?");

    message[30] = MBOX_TAG_GET_PITCH;
    message[31] = 4;
    message[32] = 4;
    message[33] = 0;

    message[34] = 0;

    if (!call(ARM_TO_VIDEOCORE_CHANNEL, message)) {
        warnln("Mailbox::init_framebuffer(): Mailbox send failed.");
        return false;
    }

    if (message[5] != width || message[6] != height) {
        warnln("Mailbox::init_framebuffer(): Setting physical dimension failed.");
        return false;
    }

    if (message[10] != width || message[11] != height) {
        warnln("Mailbox::init_framebuffer(): Setting virtual dimension failed.");
        return false;
    }

    if (message[15] != offset_x || message[16] != offset_y) {
        warnln("Mailbox::init_framebuffer(): Setting virtual offset failed.");
        return false;
    }

    if (message[20] != depth) {
        warnln("Mailbox::init_framebuffer(): Setting depth failed.");
        return false;
    }

    if (message[24] != pixel_mode) {
        warnln("Mailbox::init_framebuffer(): Setting pixel mode failed.");
        return false;
    }

    if (message[28] == 0 || message[29] == 0) {
        warnln("Mailbox::init_framebuffer(): Allocating buffer failed.");
        return false;
    }

    if (message[33] == 0) {
        warnln("Mailbox::init_framebuffer(): Retrieving pitch failed.");
        return false;
    }

    // Convert GPU address space to RAM
    *buffer_ptr = reinterpret_cast<u8*>(message[28] & 0x3FFFFFFF);

    *size = message[29];
    *pitch = message[33];

    return true;
}
}
