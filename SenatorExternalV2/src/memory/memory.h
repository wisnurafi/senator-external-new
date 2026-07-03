#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <memory>
#include <cstddef>

extern "C" intptr_t
Luck_ReadVirtualMemory
(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToRead,
    PULONG NumberOfBytesRead
);

extern "C" intptr_t
Luck_WriteVirtualMemory
(
    HANDLE Processhandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToWrite,
    PULONG NumberOfBytesWritten
);

class memory_t final
{
public:
    memory_t() = default;
    ~memory_t() = default;

    std::uint32_t find_process_id(const std::string& process_name);
    std::uint64_t find_module_address(const std::string& module_name);

    bool attach_to_process(const std::string& process_name);

    std::string read_string(std::uint64_t address);
    void write_string(std::uint64_t address, const std::string& value);

    void write_buffer(std::uint64_t address, const void* buffer, std::size_t size);

    template <typename T>
    T read(std::uint64_t address);

    template <typename T>
    bool try_read(std::uint64_t address, T& out);

    template <typename T>
    void write(std::uint64_t address, T value);

    std::uint32_t get_process_id();
    std::uint64_t get_module_address();
    HANDLE get_process_handle();
private:
    std::uint32_t process_id{};
    std::uint64_t base_address{};
    HANDLE process_handle{};
};

template <typename T>
T memory_t::read(uint64_t address)
{
    T buffer{};

    Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(address), &buffer, static_cast<ULONG>(sizeof(T)), nullptr);

    return buffer;
}

template <typename T>
bool memory_t::try_read(uint64_t address, T& out)
{
    out = T{};
    ULONG bytes_read = 0;
    Luck_ReadVirtualMemory(process_handle, reinterpret_cast<void*>(address), &out, static_cast<ULONG>(sizeof(T)), &bytes_read);
    return bytes_read == static_cast<ULONG>(sizeof(T));
}

template <typename T>
void memory_t::write(uint64_t address, T value)
{
    Luck_WriteVirtualMemory(process_handle, reinterpret_cast<void*>(address), &value, static_cast<ULONG>(sizeof(T)), nullptr);
}

extern std::unique_ptr<memory_t> memory;