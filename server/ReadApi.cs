using Npgsql;

namespace Locus.Server;

internal static class ReadApi
{
    internal static void Map(WebApplication app)
    {
        app.MapGet("/health/live", () => Results.Ok(new { status = "ok" }));
        app.MapGet("/health/ready", async (NpgsqlDataSource db, CancellationToken ct) =>
        {
            try
            {
                return await Database.ReadyAsync(db, ct)
                    ? Results.Ok(new { status = "ready", schemaVersion = Database.SchemaVersion })
                    : Results.StatusCode(503);
            }
            catch (NpgsqlException) { return Results.StatusCode(503); }
            catch (TimeoutException) { return Results.StatusCode(503); }
        });

        var v1 = app.MapGroup("/api/v1");
        v1.MapGet("/maps", Maps);
        v1.MapGet("/players/{id:guid}", Player);
        v1.MapGet("/revisions/{id:guid}/leaderboard", Leaderboard);
    }

    private sealed record MapEntry(Guid RevisionId, Guid DifficultyId, Guid SetId,
        string Title, string Artist, string Difficulty, double Stars,
        int DifficultyVersion, int KeyPresses, long AudioDurationMs, bool Ranked);

    private static async Task<IResult> Maps(NpgsqlDataSource db, Guid? after,
        int? limit, CancellationToken ct)
    {
        var count = limit ?? 50;
        if (count is < 1 or > 100) return Results.BadRequest(new { error = "limit must be 1..100" });
        await using var command = db.CreateCommand("""
            SELECT r.id, d.id, s.id, s.title, s.artist, r.title,
                   r.stars, r.difficulty_version, r.key_presses, r.audio_duration_ms, r.ranked
            FROM revisions r JOIN difficulties d ON d.id=r.difficulty_id
            JOIN song_sets s ON s.id=d.set_id
            WHERE r.is_current AND r.publication='published'
              AND (@after IS NULL OR r.id > @after)
            ORDER BY r.id LIMIT @limit
            """);
        command.Parameters.Add("after", NpgsqlTypes.NpgsqlDbType.Uuid).Value = (object?)after ?? DBNull.Value;
        command.Parameters.AddWithValue("limit", count + 1);
        await using var rows = await command.ExecuteReaderAsync(ct);
        var items = new List<MapEntry>(count + 1);
        while (await rows.ReadAsync(ct))
            items.Add(new(rows.GetGuid(0), rows.GetGuid(1), rows.GetGuid(2), rows.GetString(3),
                rows.GetString(4), rows.GetString(5), rows.GetDouble(6), rows.GetInt32(7),
                rows.GetInt32(8), rows.GetInt64(9), rows.GetBoolean(10)));
        var more = items.Count > count;
        if (more) items.RemoveAt(count);
        return Results.Ok(new { items, next = more ? items[^1].RevisionId : (Guid?)null });
    }

    private static async Task<IResult> Player(Guid id, NpgsqlDataSource db, CancellationToken ct)
    {
        await using var command = db.CreateCommand("""
            SELECT id, display_name, created_at FROM players WHERE id=@id AND active
            """);
        command.Parameters.AddWithValue("id", id);
        await using var row = await command.ExecuteReaderAsync(ct);
        if (!await row.ReadAsync(ct)) return Results.NotFound();
        return Results.Ok(new { id = row.GetGuid(0), displayName = row.GetString(1),
            createdAt = row.GetDateTime(2) });
    }

    private sealed record ScoreEntry(Guid AttemptId, Guid PlayerId, string DisplayName,
        double Pp, double AccuracyPercent, int MaxCombo, int RatePercent, int ModFlags);

    private static async Task<IResult> Leaderboard(Guid id, NpgsqlDataSource db,
        int? limit, CancellationToken ct)
    {
        var count = limit ?? 50;
        if (count is < 1 or > 100) return Results.BadRequest(new { error = "limit must be 1..100" });
        // One read transaction gives the visibility check and list one snapshot.
        await using var connection = await db.OpenConnectionAsync(ct);
        await using var tx = await connection.BeginTransactionAsync(System.Data.IsolationLevel.RepeatableRead, ct);
        await using var visible = new NpgsqlCommand(
            "SELECT EXISTS(SELECT 1 FROM revisions WHERE id=@id AND publication='published' AND ranked)",
            connection, tx);
        visible.Parameters.AddWithValue("id", id);
        if (await visible.ExecuteScalarAsync(ct) is not true) return Results.NotFound();
        await using var command = new NpgsqlCommand("""
            SELECT attempt_id, player_id, display_name, pp, accuracy_percent, max_combo, rate_percent, mod_flags
            FROM (
                SELECT DISTINCT ON (a.player_id) a.id AS attempt_id, a.player_id, p.display_name,
                    v.pp, v.accuracy_percent, v.max_combo, a.rate_percent, a.mod_flags
                FROM verified_scores v JOIN attempts a ON a.id=v.attempt_id
                JOIN players p ON p.id=a.player_id
                WHERE a.revision_id=@id AND v.rank_approved AND p.active
                  AND v.performance_version=1
                ORDER BY a.player_id, v.pp DESC, v.accuracy_percent DESC, a.id
            ) best ORDER BY pp DESC, accuracy_percent DESC, attempt_id LIMIT @limit
            """, connection, tx);
        command.Parameters.AddWithValue("id", id);
        command.Parameters.AddWithValue("limit", count);
        await using var rows = await command.ExecuteReaderAsync(ct);
        var items = new List<ScoreEntry>(count);
        while (await rows.ReadAsync(ct))
            items.Add(new(rows.GetGuid(0), rows.GetGuid(1), rows.GetString(2), rows.GetDouble(3),
                rows.GetDouble(4), rows.GetInt32(5), rows.GetInt32(6), rows.GetInt32(7)));
        await rows.DisposeAsync();
        await tx.CommitAsync(ct);
        return Results.Ok(new { revisionId = id, performanceVersion = 1, items });
    }
}
