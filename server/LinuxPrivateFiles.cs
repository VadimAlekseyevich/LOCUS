using System.Runtime.InteropServices;

namespace Locus.Server;

// Linux fixed-width statx ABI. All paths must be in exclusively owned
// directories, never concurrently writable by a worker or untrusted user.
internal static class LinuxPrivateFiles
{
    internal const UnixFileMode DirectoryMode = UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute;
    internal const UnixFileMode PendingMode = UnixFileMode.UserRead | UnixFileMode.UserWrite;
    internal const UnixFileMode ObjectMode = UnixFileMode.UserRead;

    #pragma warning disable CS0649 // Native statx fills the structure.
    [StructLayout(LayoutKind.Explicit, Size = 256)]
    private struct Statx
    {
        [FieldOffset(0)] public uint Mask;
        [FieldOffset(16)] public uint Links;
        [FieldOffset(20)] public uint Uid;
        [FieldOffset(28)] public ushort Mode;
        [FieldOffset(40)] public ulong Size;
    }
    #pragma warning restore CS0649

    [DllImport("libc", EntryPoint = "statx", SetLastError = true)]
    private static extern int Stat(int dirfd, [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        int flags, uint mask, out Statx value);
    [DllImport("libc", EntryPoint = "geteuid")]
    private static extern uint EffectiveUid();
    [DllImport("libc", EntryPoint = "open", SetLastError = true)]
    private static extern int Open([MarshalAs(UnmanagedType.LPUTF8Str)] string path, int flags);
    [DllImport("libc", EntryPoint = "fsync", SetLastError = true)]
    private static extern int Fsync(int fd);
    [DllImport("libc", EntryPoint = "close")]
    private static extern int Close(int fd);
    [DllImport("libc", EntryPoint = "renameat2", SetLastError = true)]
    private static extern int Rename(int oldDir, [MarshalAs(UnmanagedType.LPUTF8Str)] string source,
        int newDir, [MarshalAs(UnmanagedType.LPUTF8Str)] string target, uint flags);

    private static Statx Read(string path)
    {
        const uint fields = 0x001 | 0x002 | 0x004 | 0x008 | 0x200;
        if (!OperatingSystem.IsLinux() ||
            Stat(-100, path, 0x100 | 0x800, fields, out var s) != 0 || (s.Mask & fields) != fields)
            throw new IOException("Cannot inspect private path.");
        return s;
    }

    internal static long RegularSize(string path, long max, bool stored = false)
    {
        var s = Read(path);
        if ((s.Mode & 0xf000) != 0x8000 || s.Links != 1 || s.Size == 0 ||
            max <= 0 || s.Size > (ulong)max ||
            (stored && (s.Uid != EffectiveUid() || (s.Mode & 0xfff) != (ushort)ObjectMode)))
            throw new IOException("Not an exclusive bounded regular file.");
        return (long)s.Size;
    }

    internal static void RequirePrivateDirectory(string path)
    {
        var s = Read(path);
        if (EffectiveUid() == 0 || (s.Mode & 0xf000) != 0x4000 ||
            s.Uid != EffectiveUid() || (s.Mode & 0xfff) != (ushort)DirectoryMode)
            throw new IOException("Storage directory must be private and operator-owned.");
    }

    // No replacement, even for two concurrent writers of the same object.
    // Unsupported filesystem/kernel fails closed; no non-atomic fallback.
    internal static bool Publish(string source, string target)
    {
        if (Rename(-100, source, -100, target, 1) == 0) return true; // RENAME_NOREPLACE
        if (Marshal.GetLastPInvokeError() == 17) return false; // EEXIST
        throw new IOException("Atomic object commit failed.");
    }

    internal static void SyncDirectory(string path)
    {
        var fd = Open(path, 0x10000 | 0x20000 | 0x80000); // O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC
        if (fd < 0) throw new IOException("Cannot open storage directory.");
        try
        {
            if (Fsync(fd) != 0) throw new IOException("Cannot synchronize storage directory.");
        }
        finally { _ = Close(fd); }
    }
}
