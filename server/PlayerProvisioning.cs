using Npgsql;

namespace Locus.Server;

internal static class PlayerProvisioning
{
    // Private operator command, not an HTTP endpoint. No provider admin
    // password or management token is needed by the resource server.
    internal static async Task<Guid> CreateAsync(NpgsqlDataSource db, string? issuer,
        string subject, string name, CancellationToken ct)
    {
        if (!PlayerAuth.ValidIssuer(issuer) || !PlayerAuth.ValidSubject(subject))
            throw new ArgumentException("Invalid configured issuer or subject.");
        // Deliberate v1 public handle policy: ASCII avoids ambiguous Unicode
        // normalization. Unicode chart metadata is unaffected.
        if (name.Length is < 3 or > 24 ||
            name.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_')))
            throw new ArgumentException("Player name must be 3..24 ASCII letters, digits or underscores.");

        var id = Guid.NewGuid();
        await using var connection = await db.OpenConnectionAsync(ct);
        await using var tx = await connection.BeginTransactionAsync(ct);
        await using var player = new NpgsqlCommand("""
            INSERT INTO players(id, display_name, normalized_name)
            VALUES (@id, @name, @normalized)
            """, connection, tx);
        player.Parameters.AddWithValue("id", id);
        player.Parameters.AddWithValue("name", name);
        player.Parameters.AddWithValue("normalized", name.ToLowerInvariant());
        await player.ExecuteNonQueryAsync(ct);

        await using var identity = new NpgsqlCommand("""
            INSERT INTO player_identities(issuer, subject, player_id)
            VALUES (@issuer, @subject, @id)
            """, connection, tx);
        identity.Parameters.AddWithValue("issuer", issuer!);
        identity.Parameters.AddWithValue("subject", subject);
        identity.Parameters.AddWithValue("id", id);
        await identity.ExecuteNonQueryAsync(ct);
        await tx.CommitAsync(ct);
        return id;
    }
}
