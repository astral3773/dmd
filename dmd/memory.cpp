#include "memory.h"
#include <array>

namespace dmd {
    bool Memory::Setup() {
        std::array<LPCSTR, 2> args("-device", "fpga");

        handle_ = VMMDLL_Initialize(args.size(), args.data());

        return handle_ != NULL && handle_ != INVALID_HANDLE_VALUE;
    }

    bool Memory::Attach(const std::string& process_name) {
        return VMMDLL_PidGetFromName(handle_, process_name.c_str(), &process_id_);
    }

    bool Memory::ReadMemory(void* src, void* dst, size_t size) {
        return VMMDLL_MemRead(handle_, )
    }
}
