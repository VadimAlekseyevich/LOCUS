using System.Reflection;
using System.Security.Cryptography;
using Npgsql;

namespace Locus.Server;

internal static class Database
{
    internal const int SchemaVersion = 1;
    private const string Resource = "Locus.Server.Migrations.001_initial.sql";
    internal static NpgsqlDataSource Connect(string? connectionString) =>
        NpgsqlDataSource.Create(!string.IsNullOrWhiteSpace(connectionString)
            ? connectionString : throw new InvalidOperationException("ConnectionStrings__Locus is required."));

    // Explicit operator command, not an implicit web startup side effect.
    // Advisory lock and transaction make concurrent migration attempts safe.
    internal static async Task MigrateAsync(NpgsqlDataSource source, CancellationToken ct)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(Resource)
            ?? throw new InvalidOperationException("Migration resource missing.");
        using var reader = new StreamReader(stream);
        var sql = await reader.ReadToEndAsync(ct);
        var checksum = Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(sql)));
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
        await using var rows = await current.ExecuteReaderAsync(ct);
        var applied = false;
        while (await rows.ReadAsync(ct))
        {
            if (rows.GetInt32(0) != SchemaVersion || rows.GetString(1) != checksum)
                throw new InvalidOperationException("Database migration history does not match this build.");
            applied = true;
        }
        await rows.DisposeAsync();
        if (!applied)
        {
            await Execute(sql);
            await using var insert = new NpgsqlCommand(
                "INSERT INTO schema_migrations(version, checksum) VALUES (@version, @checksum)", connection, tx);
            insert.Parameters.AddWithValue("version", SchemaVersion);
            insert.Parameters.AddWithValue("checksum", checksum);
            await insert.ExecuteNonQueryAsync(ct);
        }
        await tx.CommitAsync(ct);
    }

    internal static async Task<bool> ReadyAsync(NpgsqlDataSource source, CancellationToken ct)
    {
        await using var command = source.CreateCommand(
            "SELECT count(*) = 1 AND max(version) = @version FROM schema_migrations");
        command.Parameters.AddWithValue("version", SchemaVersion);
        return await command.ExecuteScalarAsync(ct) is true;
    }
}
