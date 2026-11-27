using System.Security.Claims;
using System.Text;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.AspNetCore.Authorization;
using Microsoft.IdentityModel.Tokens;
using Npgsql;

namespace Locus.Server;

internal static class PlayerAuth
{
    internal const string Policy = "active-player";
    internal const string Scope = "locus.player";

    internal static bool ValidIssuer(string? issuer) =>
        !string.IsNullOrWhiteSpace(issuer) && Encoding.UTF8.GetByteCount(issuer) <= 512 &&
        Uri.TryCreate(issuer, UriKind.Absolute, out var uri) &&
        uri.Scheme == Uri.UriSchemeHttps && uri.UserInfo.Length == 0 &&
        uri.Query.Length == 0 && uri.Fragment.Length == 0;

    internal static bool ValidSubject(string? subject) =>
        !string.IsNullOrEmpty(subject) && subject.Length <= 255 &&
        subject.All(c => c >= 0x21 && c <= 0x7e);

    internal static void Configure(WebApplicationBuilder builder)
    {
        var issuer = builder.Configuration["Auth:Issuer"];
        var audience = builder.Configuration["Auth:Audience"];
        var enabled = issuer is not null || audience is not null;
        if (enabled && (!ValidIssuer(issuer) ||
            string.IsNullOrWhiteSpace(audience) || audience.Length > 256 ||
            audience.Any(char.IsWhiteSpace)))
            throw new InvalidOperationException("Auth requires an HTTPS issuer and an explicit API audience.");

        builder.Services.AddAuthentication(JwtBearerDefaults.AuthenticationScheme)
            .AddJwtBearer(options =>
            {
                options.MapInboundClaims = false;
                options.IncludeErrorDetails = false;
                options.SaveToken = false;
                options.RequireHttpsMetadata = true;
                if (!enabled)
                {
                    // Read-only private development without an issuer. Never
                    // accept a token or contact metadata in this mode.
                    options.Events.OnMessageReceived = context =>
                    {
                        context.NoResult();
                        return Task.CompletedTask;
                    };
                    return;
                }
                options.Authority = issuer;
                options.Audience = audience;
                options.BackchannelTimeout = TimeSpan.FromSeconds(10);
                options.TokenValidationParameters = new TokenValidationParameters
                {
                    ValidateIssuer = true,
                    ValidIssuer = issuer,
                    // Do not implicitly trust a different issuer advertised by discovery.
                    IssuerValidator = (value, _, _) => string.Equals(value, issuer, StringComparison.Ordinal)
                        ? value : throw new SecurityTokenInvalidIssuerException("Unexpected issuer."),
                    ValidateAudience = true,
                    ValidAudience = audience,
                    IgnoreTrailingSlashWhenValidatingAudience = false,
                    RequireSignedTokens = true,
                    ValidateIssuerSigningKey = true,
                    ValidAlgorithms = [SecurityAlgorithms.RsaSha256],
                    ValidTypes = ["at+jwt", "application/at+jwt"],
                    ValidateLifetime = true,
                    RequireExpirationTime = true,
                    ClockSkew = TimeSpan.FromSeconds(30)
                };
            });

        var policy = new AuthorizationPolicyBuilder(JwtBearerDefaults.AuthenticationScheme)
            .RequireAuthenticatedUser()
            .AddRequirements(new ActivePlayerRequirement())
            .Build();
        builder.Services.AddAuthorizationBuilder()
            .AddPolicy(Policy, policy)
            .SetDefaultPolicy(policy)
            .SetFallbackPolicy(policy);
        builder.Services.AddScoped<CurrentPlayer>();
        builder.Services.AddScoped<IAuthorizationHandler, ActivePlayerHandler>();
    }

    internal static void Map(WebApplication app)
    {
        app.MapGet("/api/v1/me", (CurrentPlayer player) =>
            Results.Ok(player.Value ?? throw new InvalidOperationException("Missing authorized player.")))
            .RequireAuthorization(Policy);
    }

    private sealed class ActivePlayerRequirement : IAuthorizationRequirement { }

    private sealed class ActivePlayerHandler(NpgsqlDataSource db, CurrentPlayer current,
        IHttpContextAccessor http) : AuthorizationHandler<ActivePlayerRequirement>
    {
        protected override async Task HandleRequirementAsync(AuthorizationHandlerContext context,
            ActivePlayerRequirement requirement)
        {
            if (context.User.Identity?.IsAuthenticated != true ||
                context.User.HasClaim(c => c.Type == "act")) return;
            var issuer = SingleClaim(context.User, "iss");
            var subject = SingleClaim(context.User, "sub");
            var scope = SingleClaim(context.User, "scope");
            if (issuer is null || Encoding.UTF8.GetByteCount(issuer) > 512 ||
                !ValidSubject(subject) ||
                scope is null || !scope.Split(' ', StringSplitOptions.RemoveEmptyEntries)
                    .Contains(Scope, StringComparer.Ordinal))
                return;

            var request = http.HttpContext;
            if (request is null) return;
            // No positive cache: account disable/unlink is effective on the
            // next authorization decision even for an unexpired access token.
            await using var command = db.CreateCommand("""
                SELECT p.id, p.display_name, p.created_at
                FROM player_identities i JOIN players p ON p.id = i.player_id
                WHERE i.issuer = @issuer AND i.subject = @subject AND p.active
                """);
            command.Parameters.AddWithValue("issuer", issuer);
            command.Parameters.AddWithValue("subject", subject!);
            await using var row = await command.ExecuteReaderAsync(request.RequestAborted);
            if (!await row.ReadAsync(request.RequestAborted)) return;
            current.Value = new(row.GetGuid(0), row.GetString(1), row.GetDateTime(2));
            context.Succeed(requirement);
        }

        private static string? SingleClaim(ClaimsPrincipal user, string name)
        {
            using var claims = user.FindAll(name).GetEnumerator();
            if (!claims.MoveNext()) return null;
            var value = claims.Current.Value;
            return claims.MoveNext() ? null : value;
        }
    }
}

internal sealed record PlayerIdentity(Guid Id, string DisplayName, DateTime CreatedAt);
internal sealed class CurrentPlayer
{
    internal PlayerIdentity? Value { get; set; }
}
