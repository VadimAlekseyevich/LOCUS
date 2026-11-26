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

var builder = WebApplication.CreateBuilder(args);
if (string.IsNullOrEmpty(builder.Configuration["urls"]))
    builder.WebHost.UseUrls("http://127.0.0.1:5080");
builder.Services.AddProblemDetails();
builder.Services.AddSingleton<NpgsqlDataSource>(_ => Database.Connect(builder.Configuration.GetConnectionString("Locus")));
var app = builder.Build();
// Resolve configuration immediately, not on the first public request.
_ = app.Services.GetRequiredService<NpgsqlDataSource>();
app.UseExceptionHandler();
ReadApi.Map(app);
await app.RunAsync();
return 0;
