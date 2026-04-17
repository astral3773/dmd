#include "memory.h"
#include <array>

namespace dmd {
    bool Memory::Setup() {
        std::array<LPCSTR, 2> args{ "-device", "fpga" };

        handle_ = VMMDLL_Initialize(args.size(), args.data());

        return handle_ != NULL && handle_ != INVALID_HANDLE_VALUE;
    }

    bool Memory::Attach(const std::string& process_name) {
        return VMMDLL_PidGetFromName(handle_, process_name.c_str(), &process_id_);
    }

    std::optional<Range> Memory::GetImage(const std::string& image_name) {
        PVMMDLL_MAP_MODULEENTRY entry = nullptr;
        if (!VMMDLL_Map_GetModuleFromNameU(handle_, process_id_, image_name.c_str(), &entry, 0) || !entry) {
            return std::nullopt;
        }

        return Range(entry->vaBase, entry->vaBase + entry->cbImageSize);
    }

    bool Memory::ReadMemory(void* src, void* dst, size_t size) {
        DWORD offset = 0;
        bool any_ok = false;

        while (offset < size) {
            uint64_t addr = reinterpret_cast<uint64_t>(src) + offset;
            size_t page_off = addr & (0x1000 - 1);
            DWORD chunk = static_cast<DWORD>(min(
                static_cast<size_t>(size - offset),
                page_off ? 0x1000 - page_off : static_cast<size_t>(0x1000)
            ));

            DWORD bytes_read = 0;
            if (VMMDLL_MemReadEx(handle_, process_id_, addr,
                reinterpret_cast<uint8_t*>(dst) + offset, chunk, &bytes_read,
                VMMDLL_FLAG_NOPAGING_IO | VMMDLL_FLAG_NOCACHEPUT | VMMDLL_FLAG_NOMEMCALLBACK)
                && bytes_read == chunk)
                any_ok = true;

            offset += chunk;
        }

        return any_ok;
    }
}
