#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include "../includes/vmmdll.h"

namespace dmd {

	class Range {
	public:
		Range(uint64_t begin, uint64_t end) : begin_(begin), end_(end) {

		}

		uint64_t Begin() const {
			return begin_;
		}

		uint64_t End() const {
			return end_;
		}

		size_t Size() const {
			return end_ - begin_;
		}

		bool Contains(uint64_t address) const {
			return Begin() <= address && End() > address;
		}
	private:

		uint64_t begin_ = 0;
		uint64_t end_ = 0;
	};
	class Memory {
	public:
		bool Setup();
		bool Attach(const std::string& process_name);

		template<typename T>
		T Read(uint64_t address) {
			T buffer{};
			(void)ReadMemory(reinterpret_cast<void*>(address), &buffer, sizeof(T));
			return buffer;
		}

	private:

		bool ReadMemory(void* src, void* dst, size_t size);

		VMM_HANDLE handle_ = NULL;
		DWORD process_id_ = NULL;
	};
}

#endif // !MEMORY_H