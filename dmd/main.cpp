#include <cstdint>
#include <string>
#include <memory>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <print>
#include <format>
#include <windows.h>
#include <shobjidl.h>
#include "memory.h"

using namespace dmd;

std::string GetOutputPath(const std::string& default_name) {
    std::string result;

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        return result;
    }

    IFileSaveDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(
        CLSID_FileSaveDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)))) {
        CoUninitialize();
        return result;
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    }

    COMDLG_FILTERSPEC filters[] = {
        {L"Executable Binaries (*.exe;*.dll;*.sys)", L"*.exe;*.dll;*.sys"},
        {L"Executable Files (*.exe)", L"*.exe"},
        {L"Dynamic Link Libraries (*.dll)", L"*.dll"},
        {L"System Files (*.sys)", L"*.sys"}
    };

    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);

    const auto current_dir = std::filesystem::current_path().wstring();

    IShellItem* folder = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(
        current_dir.c_str(),
        nullptr,
        IID_PPV_ARGS(&folder)))) {
        dialog->SetDefaultFolder(folder);
        folder->Release();
    }

    std::wstring wide_name(default_name.begin(), default_name.end());
    dialog->SetFileName(wide_name.c_str());

    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR raw_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
                std::filesystem::path path(raw_path);
                CoTaskMemFree(raw_path);

                if (!path.has_extension()) {
                    UINT filter_index = 1;
                    if (SUCCEEDED(dialog->GetFileTypeIndex(&filter_index))) {
                        switch (filter_index) {
                        case 2:
                            path.replace_extension(".exe");
                            break;
                        case 3:
                            path.replace_extension(".dll");
                            break;
                        case 4:
                            path.replace_extension(".sys");
                            break;
                        case 1:
                        default:
                            path.replace_extension(".exe");
                            break;
                        }
                    }
                    else {
                        path.replace_extension(".exe");
                    }
                }

                result = path.string();
            }

            item->Release();
        }
    }

    dialog->Release();
    CoUninitialize();
    return result;
}

int main() {
	Memory dma;
	if (!dma.Setup()) {
		std::println("Failed to connect to DMA.");
		(void)std::getchar();
		return 1;
	}
	std::println("Connected to DMA!");

	std::println("Enter process name, eg. \"notepad.exe\"");

	std::string process_name;
	std::getline(std::cin, process_name);

	if (!dma.Attach(process_name)) {
		std::println("Failed to find {}", process_name);
		(void)std::getchar();
		return 2;
	}
	std::println("Attached to {} ({})", process_name, dma.ProcessId());

	std::println("Enter image name (leave empty for default)");

	std::string image_name;
	std::getline(std::cin, image_name);
	if (image_name.empty()) {
		image_name = process_name;
	}
	
	auto image = dma.GetImage(image_name);
	if (!image) {
		std::println("Failed to find image {}", image_name);
		(void)std::getchar();
		return 3;
	}
	std::println("Found image {}", image_name);
	std::println("Image base {:#x}", image->Begin());
	std::println("Image size {}KB", image->Size() / 1KB);

	const auto buffer = std::make_unique<uint8_t[]>(image->Size());
	if (!dma.Read(image->Begin(), buffer.get(), image->Size())) {
		//std::println("Failed to read image into buffer");
		//(void)std::getchar();
		//return 4;
	}
	std::println("Read image into buffer {:#x}", reinterpret_cast<uint64_t>(buffer.get()));

	auto* const dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer.get());
	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		std::println("Failed to find dos header (invalid signature)");
		(void)std::getchar();
		return 5;
	}
	std::println("Image dos header valid");

	auto* const nt_header =
		reinterpret_cast<PIMAGE_NT_HEADERS>(buffer.get() + dos_header->e_lfanew);
	if (nt_header->Signature != IMAGE_NT_SIGNATURE) {
		std::println("Failed to find nt headers (invalid signature)");
		(void)std::getchar();
		return 6;
	}
	std::println("Image nt headers valid");

	auto* const sections = IMAGE_FIRST_SECTION(nt_header);

	nt_header->OptionalHeader.ImageBase = image->Begin();

	std::println("Image section count {}", nt_header->FileHeader.NumberOfSections);

	for (size_t i = 0; i < nt_header->FileHeader.NumberOfSections; ++i) {
		sections[i].PointerToRawData = sections[i].VirtualAddress;
		char name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
		std::memcpy(name, sections[i].Name, IMAGE_SIZEOF_SHORT_NAME);

		std::println("Fixed section {} @ {:#x}", name, sections[i].VirtualAddress);
	}

    const std::filesystem::path image_path(image_name);

	const std::string result_path = GetOutputPath(
        std::format("{}_dump.", image_path.stem().string(), image_path.extension().string()));

	std::ofstream stream(result_path, std::ios::binary);
	stream.write(reinterpret_cast<const char*>(buffer.get()), image->Size());
	stream.close();

    std::println("Wrote {} to {}.", image_name, result_path);

	std::getchar();
	return 0;
}