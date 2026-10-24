#!/usr/bin/env python3
"""Check that 32-bit Windows programs import only what Windows 9x provides.

    check-imports.py [--dlls DIR] [--allow-unicode] FILE.exe|FILE.dll ...

Programs for Windows 9x should be native ANSI builds. Windows 95, 98 and ME
export most Unicode ("W") functions only as stubs that fail with
ERROR_CALL_NOT_IMPLEMENTED, so every W import outside the short list of ones
that work is an error (a warning with --allow-unicode). Without --dlls, the
check also knows a list of NT-only and newer kernel32 functions.

With --dlls, DIR holds one directory per Windows version (for example 95,
98se, me), each with system DLLs copied from that version (kernel32.dll,
user32.dll, gdi32.dll, advapi32.dll, msvcrt.dll, ...). Every import must then
be exported by the DLL of each version that has it. Imports from DLLs that a
version directory does not hold are listed as unchecked.

The exit status is 1 when something is missing, 0 otherwise.
"""
import argparse
import os
import struct
import sys

# W functions that Windows 95, 98 and ME implement natively (Microsoft
# Layer for Unicode documentation, "Unicode support in Windows 95").
WORKING_W = {
    "EnumResourceLanguagesW", "EnumResourceNamesW", "EnumResourceTypesW",
    "ExtTextOutW", "FindResourceExW", "FindResourceW", "GetCharWidthW",
    "GetCommandLineW", "GetTextExtentExPointW", "GetTextExtentPoint32W",
    "GetTextExtentPointW", "lstrlenW", "MessageBoxExW", "MessageBoxW",
    "MultiByteToWideChar", "TextOutW", "WideCharToMultiByte",
}

# kernel32 functions that no 9x version has: NT-only ones and later
# additions. A program that imports one of them does not load on 9x.
NOT_ON_9X = {
    "AcquireSRWLockExclusive", "AcquireSRWLockShared",
    "CreateSymbolicLinkA", "CreateSymbolicLinkW", "FlsAlloc", "FlsFree",
    "FlsGetValue", "FlsSetValue", "GetFileInformationByHandleEx",
    "GetFinalPathNameByHandleA", "GetFinalPathNameByHandleW",
    "GetNativeSystemInfo", "GetProcessId", "GetSystemTimePreciseAsFileTime",
    "GetThreadId", "GetTickCount64", "InitOnceBeginInitialize",
    "InitOnceComplete", "InitOnceExecuteOnce", "InitializeConditionVariable",
    "InitializeSRWLock", "ReleaseSRWLockExclusive", "ReleaseSRWLockShared",
    "SetFileInformationByHandle", "SignalObjectAndWait",
    "SleepConditionVariableCS", "SleepConditionVariableSRW", "SwitchToThread",
    "TryAcquireSRWLockExclusive", "TryAcquireSRWLockShared",
    "TryEnterCriticalSection", "WaitOnAddress", "WakeAllConditionVariable",
    "WakeByAddressAll", "WakeByAddressSingle", "WakeConditionVariable",
}

SYSTEM_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll",
               "shell32.dll", "comctl32.dll", "comdlg32.dll", "ole32.dll",
               "oleaut32.dll", "winmm.dll", "wsock32.dll", "ws2_32.dll",
               "version.dll", "mpr.dll", "ntdll.dll"}


class PE:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if d[:2] != b"MZ":
            raise ValueError("not an MZ file")
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise ValueError("not a PE file")
        nsect, = struct.unpack_from("<H", d, pe + 6)
        optsize, = struct.unpack_from("<H", d, pe + 20)
        opt = pe + 24
        magic, = struct.unpack_from("<H", d, opt)
        if magic != 0x10B:
            raise ValueError("not a 32-bit PE file")
        self.subsystem_version = struct.unpack_from("<HH", d, opt + 48)
        ndirs, = struct.unpack_from("<I", d, opt + 92)
        self.dirs = [struct.unpack_from("<II", d, opt + 96 + 8 * i)
                     for i in range(min(ndirs, 16))]
        self.sections = []
        s = opt + optsize
        for i in range(nsect):
            vsize, va, rawsize, rawptr = struct.unpack_from(
                "<IIII", d, s + 40 * i + 8)
            self.sections.append((va, max(vsize, rawsize), rawptr))

    def off(self, rva):
        for va, size, raw in self.sections:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError("RVA 0x%x is outside every section" % rva)

    def cstr(self, rva):
        o = self.off(rva)
        return self.data[o:self.data.index(b"\0", o)].decode("latin-1")

    def imports(self):
        """Yield (dll, name) pairs; name is "#ordinal" for ordinal imports."""
        if len(self.dirs) < 2 or not self.dirs[1][0]:
            return
        o = self.off(self.dirs[1][0])
        while True:
            ilt, _, _, name, iat = struct.unpack_from("<IIIII", self.data, o)
            if not name:
                break
            dll = self.cstr(name)
            t = self.off(ilt or iat)
            while True:
                e, = struct.unpack_from("<I", self.data, t)
                if not e:
                    break
                if e & 0x80000000:
                    yield dll, "#%d" % (e & 0xFFFF)
                else:
                    yield dll, self.cstr(e + 2)
                t += 4
            o += 20

    def exports(self):
        """Return the set of exported names and "#ordinal" strings."""
        out = set()
        if not self.dirs or not self.dirs[0][0]:
            return out
        o = self.off(self.dirs[0][0])
        base, nfuncs, nnames, _, names = struct.unpack_from(
            "<IIIII", self.data, o + 16)
        for i in range(nfuncs):
            out.add("#%d" % (base + i))
        if nnames:
            n = self.off(names)
            for i in range(nnames):
                out.add(self.cstr(struct.unpack_from("<I", self.data,
                                                     n + 4 * i)[0]))
        return out


def is_unicode_stub(ldll, name, versions):
    """Whether NAME is a W function that Windows 9x does not implement."""
    if ldll not in SYSTEM_DLLS or not name.endswith("W") or name in WORKING_W:
        return False
    if versions is None:
        return True
    # With real DLLs, a W function is one whose A twin exists too.
    twin = name[:-1] + "A"
    return any(twin in dlls.get(ldll, ()) for dlls in versions.values())


def load_versions(root):
    versions = {}
    for v in sorted(os.listdir(root)):
        vdir = os.path.join(root, v)
        if not os.path.isdir(vdir):
            continue
        dlls = {}
        for f in os.listdir(vdir):
            if f.lower().endswith((".dll", ".drv", ".exe")):
                dlls[f.lower()] = PE(os.path.join(vdir, f)).exports()
        versions[v] = dlls
    return versions


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dlls", help="directory with one subdirectory of "
                    "system DLLs per Windows version")
    ap.add_argument("--allow-unicode", action="store_true",
                    help="report Unicode stubs as warnings, for programs "
                    "that check for them at run time")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()
    versions = load_versions(args.dlls) if args.dlls else None
    bad = False
    for path in args.files:
        pe = PE(path)
        major, minor = pe.subsystem_version
        print("%s: subsystem version %d.%02d" % (path, major, minor))
        if major > 4:
            print("  error: Windows 9x loads only subsystem version 4.x "
                  "or older")
            bad = True
        unchecked = set()
        absent = {}
        for dll, name in pe.imports():
            ldll = dll.lower()
            if ldll == "kernel32.dll" and name in NOT_ON_9X:
                print("  error: %s!%s does not exist on Windows 9x"
                      % (dll, name))
                bad = True
                continue
            if is_unicode_stub(ldll, name, versions):
                level = "warning" if args.allow_unicode else "error"
                print("  %s: %s!%s is a Unicode function, which Windows 9x "
                      "only has as a stub; use the A function"
                      % (level, dll, name))
                bad = bad or not args.allow_unicode
            if versions is None:
                continue
            missing = [v for v, dlls in versions.items()
                       if ldll in dlls and name not in dlls[ldll]]
            if not any(ldll in dlls for dlls in versions.values()):
                unchecked.add(dll)
            else:
                for v, dlls in versions.items():
                    if ldll not in dlls:
                        absent.setdefault(dll, set()).add(v)
            if missing:
                print("  error: %s!%s is missing on %s"
                      % (dll, name, ", ".join(missing)))
                bad = True
        for dll in sorted(absent):
            print("  note: %s is not in %s: install or ship it there"
                  % (dll, ", ".join(sorted(absent[dll]))))
        for dll in sorted(unchecked):
            print("  note: no copy of %s to check against" % dll)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
