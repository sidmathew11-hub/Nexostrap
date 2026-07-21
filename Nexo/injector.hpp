#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

namespace nexo {

class Injector {
public:
    bool ManualMap(HANDLE hProc, const std::string& dllPath) {
        std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<BYTE> buffer((size_t)size);
        file.read((char*)buffer.data(), size);
        file.close();

        return ManualMapBuffer(hProc, buffer.data(), buffer.size());
    }

    bool ManualMapBuffer(HANDLE hProc, BYTE* rawBuffer, size_t bufSize) {
        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)rawBuffer;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(rawBuffer + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

        IMAGE_OPTIONAL_HEADER64& optHeader = ntHeaders->OptionalHeader;
        if (optHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;

        SIZE_T imageSize = optHeader.SizeOfImage;
        LPVOID remoteBase = VirtualAllocEx(hProc, NULL, imageSize, 
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteBase) return false;

        std::vector<BYTE> localImage(imageSize);
        memcpy(localImage.data(), rawBuffer, optHeader.SizeOfHeaders);

        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
            memcpy(localImage.data() + section[i].VirtualAddress,
                   rawBuffer + section[i].PointerToRawData,
                   section[i].SizeOfRawData);
        }

        if (!ProcessRelocations(localImage.data(), (BYTE*)remoteBase, ntHeaders)) {
            VirtualFreeEx(hProc, remoteBase, 0, MEM_RELEASE);
            return false;
        }

        if (!ResolveImports(localImage.data(), ntHeaders)) {
            VirtualFreeEx(hProc, remoteBase, 0, MEM_RELEASE);
            return false;
        }

        if (!WriteProcessMemory(hProc, remoteBase, localImage.data(), imageSize, NULL)) {
            VirtualFreeEx(hProc, remoteBase, 0, MEM_RELEASE);
            return false;
        }

        section = IMAGE_FIRST_SECTION(ntHeaders);
        for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
            DWORD oldProtect = 0;
            DWORD newProtect = PAGE_EXECUTE_READ;

            if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE)
                    newProtect = PAGE_EXECUTE_READWRITE;
                else
                    newProtect = PAGE_EXECUTE_READ;
            } else if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
                newProtect = PAGE_READWRITE;
            } else {
                newProtect = PAGE_READONLY;
            }

            VirtualProtectEx(hProc, (LPBYTE)remoteBase + section[i].VirtualAddress,
                section[i].Misc.VirtualSize, newProtect, &oldProtect);
        }

        LPVOID entryPoint = (LPBYTE)remoteBase + optHeader.AddressOfEntryPoint;

        HANDLE hThread = CreateRemoteThread(hProc, NULL, 0,
            (LPTHREAD_START_ROUTINE)entryPoint, remoteBase, 0, NULL);

        if (!hThread) {
            VirtualFreeEx(hProc, remoteBase, 0, MEM_RELEASE);
            return false;
        }

        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);

        return true;
    }

private:
    bool ProcessRelocations(BYTE* localBase, BYTE* remoteBase, PIMAGE_NT_HEADERS ntHeaders) {
        IMAGE_DATA_DIRECTORY& relocDir = 
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir.Size == 0) return true;

        uintptr_t delta = (uintptr_t)remoteBase - ntHeaders->OptionalHeader.ImageBase;
        if (delta == 0) return true;

        PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(localBase + relocDir.VirtualAddress);

        while (reloc->VirtualAddress != 0) {
            DWORD numEntries = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD entry = (PWORD)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));

            for (DWORD i = 0; i < numEntries; i++) {
                WORD type = (entry[i] >> 12) & 0xF;
                WORD offset = entry[i] & 0xFFF;

                if (type == IMAGE_REL_BASED_DIR64) {
                    uintptr_t* addr = (uintptr_t*)(localBase + reloc->VirtualAddress + offset);
                    *addr += delta;
                } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                    DWORD* addr = (DWORD*)(localBase + reloc->VirtualAddress + offset);
                    *addr += (DWORD)delta;
                } else if (type == IMAGE_REL_BASED_HIGH) {
                    WORD* addr = (WORD*)(localBase + reloc->VirtualAddress + offset);
                    *addr += HIWORD(delta);
                } else if (type == IMAGE_REL_BASED_LOW) {
                    WORD* addr = (WORD*)(localBase + reloc->VirtualAddress + offset);
                    *addr += LOWORD(delta);
                }
            }

            reloc = (PIMAGE_BASE_RELOCATION)((BYTE*)reloc + reloc->SizeOfBlock);
        }
        return true;
    }

    bool ResolveImports(BYTE* localBase, PIMAGE_NT_HEADERS ntHeaders) {
        IMAGE_DATA_DIRECTORY& importDir = 
            ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.Size == 0) return true;

        PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(localBase + importDir.VirtualAddress);

        while (importDesc->Name != 0) {
            char* moduleName = (char*)(localBase + importDesc->Name);
            HMODULE hMod = GetModuleHandleA(moduleName);
            if (!hMod) {
                hMod = LoadLibraryA(moduleName);
                if (!hMod) {
                    importDesc++;
                    continue;
                }
            }

            PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(localBase + importDesc->FirstThunk);
            PIMAGE_THUNK_DATA origThunk = importDesc->OriginalFirstThunk ?
                (PIMAGE_THUNK_DATA)(localBase + importDesc->OriginalFirstThunk) : thunk;

            while (origThunk->u1.AddressOfData != 0) {
                uintptr_t funcAddr = 0;

                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    WORD ordinal = IMAGE_ORDINAL(origThunk->u1.Ordinal);
                    funcAddr = (uintptr_t)GetProcAddress(hMod, (LPCSTR)(ULONG_PTR)ordinal);
                } else {
                    PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)
                        (localBase + origThunk->u1.AddressOfData);
                    funcAddr = (uintptr_t)GetProcAddress(hMod, importByName->Name);
                }

                if (funcAddr) {
                    thunk->u1.Function = funcAddr;
                }

                thunk++;
                origThunk++;
            }

            importDesc++;
        }
        return true;
    }
};

} // namespace nexo
