using Locus.Server;
using Npgsql;

if (args.Length > 0 && args[0] == "worker")
{
    try { return await WorkerProcess.RunAsync(args[1..], CancellationToken.None); }
    catch (Exception) { Console.Error.WriteLine("Worker execution or protocol failed."); return 1; }
}
if (args.Length == 1 && args[0] == "migrate")
{
    try
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromMinutes(2));
        await using var db = Database.Connect(Environment.GetEnvironmentVariable("ConnectionStrings__Locus"));
        await Database.MigrateAsync(db, timeout.Token);
        Console.WriteLine("Database migration complete.");
        return 0;
    }
    catch (Exception) { Console.Error.WriteLine("Database migration failed."); return 1; }
}

if (args.Length > 0 && args[0] == "provision-player")
{
    if (args.Length != 3)
    {
        Console.Error.WriteLine("Usage: provision-player SUBJECT NAME");
        return 1;
    }
    try
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        await using var db = Database.Connect(Environment.GetEnvironmentVariable("ConnectionStrings__Locus"));
        var id = await PlayerProvisioning.CreateAsync(db, Environment.GetEnvironmentVariable("Auth__Issuer"),
            args[1], args[2], timeout.Token);
        Console.WriteLine(id);
        return 0;
    }
    catch (ArgumentException) { Console.Error.WriteLine("Invalid player identity or name."); return 1; }
    catch (PostgresException error) when (error.SqlState == PostgresErrorCodes.UniqueViolation)
    { Console.Error.WriteLine("Player name or identity already exists."); return 1; }
    catch (Exception) { Console.Error.WriteLine("Player provisioning failed."); return 1; }
}

var builder = WebApplication.CreateBuilder(args);
if (string.IsNullOrEmpty(builder.Configuration["urls"]))
    builder.WebHost.UseUrls("http://127.0.0.1:5080");
builder.Services.AddProblemDetails();
builder.Services.AddHttpContextAccessor();
PlayerAuth.Configure(builder);
builder.Services.AddSingleton<NpgsqlDataSource>(_ => Database.Connect(builder.Configuration.GetConnectionString("Locus")));
var app = builder.Build();
// Resolve configuration immediately, not on the first public request.
_ = app.Services.GetRequiredService<NpgsqlDataSource>();
app.UseExceptionHandler();
app.Use(async (context, next) =>
{
    if (context.Request.Path.Equals("/api/v1/me", StringComparison.OrdinalIgnoreCase) ||
        context.Request.Path.Equals("/api/v1/me/", StringComparison.OrdinalIgnoreCase))
        context.Response.Headers.CacheControl = "no-store";
    await next(context);
});
app.UseAuthentication();
app.UseAuthorization();
ReadApi.Map(app);
PlayerAuth.Map(app);
await app.RunAsync();
return 0;
