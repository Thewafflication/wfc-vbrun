// Checks the VBRUN compatibility library against the retained reference
// inventory (REQ-0099, REQ-0105). Usage:
//   vbrun_tests layout <dll> <expected.csv>   export directory equals the list
//   vbrun_tests load <dll>                    loads; names and ordinals resolve
//   vbrun_tests call <dll> <name>             calls an export (stops the
//   process)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace {

bool read_expected(const char* const path,
                   std::map<unsigned, std::string>& expected) {
    std::ifstream stream(path);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto comma = line.find(',');
        if (comma == std::string::npos) {
            continue;
        }
        expected[static_cast<unsigned>(std::stoul(line.substr(0, comma)))] =
            line.substr(comma + 1U);
    }
    return !expected.empty();
}

// Converts an RVA to a file offset using the section table.
bool rva_to_offset(const std::vector<unsigned char>& file,
                   const IMAGE_SECTION_HEADER* sections, const unsigned count,
                   const std::uint32_t rva, std::size_t& offset) {
    for (unsigned i = 0; i < count; ++i) {
        const auto& section = sections[i];
        const std::uint32_t size =
            section.Misc.VirtualSize > section.SizeOfRawData
                ? section.Misc.VirtualSize
                : section.SizeOfRawData;
        if (rva >= section.VirtualAddress &&
            rva < section.VirtualAddress + size) {
            offset = section.PointerToRawData + (rva - section.VirtualAddress);
            return offset < file.size();
        }
    }
    return false;
}

int check_layout(const char* const dll, const char* const csv) {
    std::map<unsigned, std::string> expected;
    if (!read_expected(csv, expected)) {
        std::fprintf(stderr, "cannot read %s\n", csv);
        return 2;
    }
    std::ifstream stream(dll, std::ios::binary);
    const std::vector<unsigned char> file(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    if (file.size() < sizeof(IMAGE_DOS_HEADER)) {
        std::fprintf(stderr, "cannot read %s\n", dll);
        return 2;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() +
                                                                 dos->e_lfanew);
    const bool wide =
        nt->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64);
    const auto* nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
    const IMAGE_DATA_DIRECTORY directory =
        wide ? nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
             : nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        reinterpret_cast<const unsigned char*>(&nt->OptionalHeader) +
        nt->FileHeader.SizeOfOptionalHeader);
    const unsigned section_count = nt->FileHeader.NumberOfSections;

    std::size_t offset = 0;
    if (directory.VirtualAddress == 0U ||
        !rva_to_offset(file, sections, section_count, directory.VirtualAddress,
                       offset)) {
        std::fprintf(stderr, "no export directory\n");
        return 1;
    }
    const auto* exports =
        reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(file.data() + offset);
    int failures = 0;
    const auto fail = [&failures](const std::string& message) {
        if (failures++ < 10) {
            std::fprintf(stderr, "%s\n", message.c_str());
        }
    };
    if (exports->Base != 100U) {
        fail("ordinal base " + std::to_string(exports->Base) + " != 100");
    }
    if (exports->NumberOfFunctions != 1911U) {
        fail("function slots " + std::to_string(exports->NumberOfFunctions) +
             " != 1911");
    }
    if (exports->NumberOfNames != expected.size()) {
        fail("named exports " + std::to_string(exports->NumberOfNames) +
             " != " + std::to_string(expected.size()));
    }

    std::size_t functions_at = 0;
    std::size_t names_at = 0;
    std::size_t ordinals_at = 0;
    if (!rva_to_offset(file, sections, section_count,
                       exports->AddressOfFunctions, functions_at) ||
        !rva_to_offset(file, sections, section_count, exports->AddressOfNames,
                       names_at) ||
        !rva_to_offset(file, sections, section_count,
                       exports->AddressOfNameOrdinals, ordinals_at)) {
        std::fprintf(stderr, "export tables are outside the image\n");
        return 1;
    }
    std::vector<std::uint32_t> functions(exports->NumberOfFunctions);
    std::vector<std::uint32_t> names(exports->NumberOfNames);
    std::vector<std::uint16_t> ordinals(exports->NumberOfNames);
    std::memcpy(functions.data(), file.data() + functions_at,
                functions.size() * sizeof(std::uint32_t));
    std::memcpy(names.data(), file.data() + names_at,
                names.size() * sizeof(std::uint32_t));
    std::memcpy(ordinals.data(), file.data() + ordinals_at,
                ordinals.size() * sizeof(std::uint16_t));

    std::map<unsigned, std::string> actual;
    for (std::size_t i = 0; i < names.size(); ++i) {
        std::size_t name_at = 0;
        if (!rva_to_offset(file, sections, section_count, names[i], name_at)) {
            fail("name " + std::to_string(i) + " is outside the image");
            continue;
        }
        actual[exports->Base + ordinals[i]] =
            reinterpret_cast<const char*>(file.data() + name_at);
    }
    for (const auto& [ordinal, name] : expected) {
        const auto found = actual.find(ordinal);
        if (found == actual.end()) {
            fail("missing export " + name + " at ordinal " +
                 std::to_string(ordinal));
        } else if (found->second != name) {
            fail("ordinal " + std::to_string(ordinal) + " is " + found->second +
                 ", expected " + name);
        }
    }
    for (std::size_t slot = 0; slot < functions.size(); ++slot) {
        const unsigned ordinal = exports->Base + static_cast<unsigned>(slot);
        const bool wanted = expected.contains(ordinal);
        if (wanted != (functions[slot] != 0U)) {
            fail(std::string(wanted ? "empty slot for"
                                    : "unexpected export at") +
                 " ordinal " + std::to_string(ordinal));
        }
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d layout differences\n", failures);
        return 1;
    }
    std::printf("layout ok: %zu named exports, %zu slots\n", expected.size(),
                functions.size());
    return 0;
}

int check_load(const char* const dll) {
    HMODULE module = LoadLibraryA(dll);
    if (module == nullptr) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    const FARPROC by_name = GetProcAddress(module, "ThunRTMain");
    const FARPROC by_ordinal = GetProcAddress(module, MAKEINTRESOURCEA(100));
    const FARPROC last = GetProcAddress(module, MAKEINTRESOURCEA(2010));
    const FARPROC hole = GetProcAddress(module, MAKEINTRESOURCEA(729));
    if (by_name == nullptr || by_name != by_ordinal || last == nullptr ||
        hole != nullptr) {
        std::fprintf(stderr, "resolution failed\n");
        return 1;
    }
    std::printf("load ok\n");
    return 0;
}

int call_export(const char* const dll, const char* const name) {
    HMODULE module = LoadLibraryA(dll);
    const FARPROC proc =
        module != nullptr ? GetProcAddress(module, name) : nullptr;
    if (proc == nullptr) {
        std::fprintf(stderr, "cannot resolve %s\n", name);
        return 1;
    }
    reinterpret_cast<void (*)()>(proc)();
    std::fprintf(stderr, "the stub returned\n");
    return 1;
}

}  // namespace

int main(const int argument_count, const char* const arguments[]) {
    if (argument_count == 4 && std::strcmp(arguments[1], "layout") == 0) {
        return check_layout(arguments[2], arguments[3]);
    }
    if (argument_count == 3 && std::strcmp(arguments[1], "load") == 0) {
        return check_load(arguments[2]);
    }
    if (argument_count == 4 && std::strcmp(arguments[1], "call") == 0) {
        return call_export(arguments[2], arguments[3]);
    }
    std::fprintf(stderr, "usage: vbrun_tests layout|load|call ...\n");
    return 2;
}
