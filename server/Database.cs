using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Npgsql;

namespace Locus.Server;

internal static class Database
{
    private sealed record Migration(int Version, string Sql, string Checksum);
    // Append-only manifest: never edit an already applied migration.
    private static readonly Migration[] Migrations =
    [
        Load(1, "001_initial.sql"),
        Load(2, "002_player_identities.sql")
    ];
    internal static int SchemaVersion => Migrations[^1].Version;

    private static Migration Load(int version, string name)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(
            $"Locus.Server.Migrations.{name}")
            ?? throw new InvalidOperationException("Migration resource missing.");
        using var reader = new StreamReader(stream);
        // Stable across Git checkout line-ending policies; v1 Linux checksum is unchanged.
        var sql = reader.ReadToEnd().Replace("\r\n", "\n", StringComparison.Ordinal);
        var checksum = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(sql)));
        return new(version, sql, checksum);
    }

    internal static NpgsqlDataSource Connect(string? connectionString) =>
        NpgsqlDataSource.Create(!string.IsNullOrWhiteSpace(connectionString)
            ? connectionString : throw new InvalidOperationException("ConnectionStrings__Locus is required."));

    // Returns the length of a valid prefix; rejects gaps, unknown versions,
    // and altered SQL before executing any pending migration.
    private static async Task<int> AppliedCountAsync(NpgsqlCommand command, CancellationToken ct)
    {
        await using var rows = await command.ExecuteReaderAsync(ct);
        var applied = 0;
        while (await rows.ReadAsync(ct))
        {
            if (applied >= Migrations.Length ||
                rows.GetInt32(0) != Migrations[applied].Version ||
                !string.Equals(rows.GetString(1), Migrations[applied].Checksum, StringComparison.Ordinal))
                return -1;
            applied++;
        }
        return applied;
    }

    // Explicit operator command. One advisory lock and one transaction protect
    // both validation and application; failures roll back all pending changes.
    internal static async Task MigrateAsync(NpgsqlDataSource source, CancellationToken ct)
    {
        await using var connection = await source.OpenConnectionAsync(ct);
        await using var tx = await connection.BeginTransactionAsync(ct);
        async Task Execute(string statement)
        {
            await using var command = new NpgsqlCommand(statement, connection, tx);
            await command.ExecuteNonQueryAsync(ct);
        }
        await Execute("SELECT pg_advisory_xact_lock(1279872085)");
        await Execute("""
            CREATE TABLE IF NOT EXISTS schema_migrations (
                version integer PRIMARY KEY,
                checksum text NOT NULL,
                applied_at timestamptz NOT NULL DEFAULT now()
            )
            """);
        await using var current = new NpgsqlCommand(
            "SELECT version, checksum FROM schema_migrations ORDER BY version", connection, tx);
        var applied = await AppliedCountAsync(current, ct);
        if (applied < 0)
            throw new InvalidOperationException("Database migration history does not match this build.");

        for (var i = applied; i < Migrations.Length; i++)
        {
            var migration = Migrations[i];
            await Execute(migration.Sql);
            await using var insert = new NpgsqlCommand(
                "INSERT INTO schema_migrations(version, checksum) VALUES (@version, @checksum)", connection, tx);
            insert.Parameters.AddWithValue("version", migration.Version);
            insert.Parameters.AddWithValue("checksum", migration.Checksum);
            await insert.ExecuteNonQueryAsync(ct);
        }
        await tx.CommitAsync(ct);
    }

    internal static async Task<bool> ReadyAsync(NpgsqlDataSource source, CancellationToken ct)
    {
        await using var command = source.CreateCommand(
            "SELECT version, checksum FROM schema_migrations ORDER BY version");
        return await AppliedCountAsync(command, ct) == Migrations.Length;
    }
}
