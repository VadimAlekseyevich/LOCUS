"""Real PostgreSQL: fresh/concurrent install, v1 upgrade and tamper rejection."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

dll = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parents[2]
base_env = os.environ.copy()

def psql(sql, database="locus", expected=True):
    result = subprocess.run(
        ["psql", "-X", "-qAt", "-v", "ON_ERROR_STOP=1",
         "-h", "127.0.0.1", "-U", "locus", "-d", database, "-c", sql],
        capture_output=True, text=True, timeout=30)
    assert (result.returncode == 0) == expected, (sql, result.stderr)
    return result.stdout.strip()

def migrate(database="locus", expected=True):
    env = dict(base_env)
    env["ConnectionStrings__Locus"] = env["ConnectionStrings__Locus"].replace(
        "Database=locus;", f"Database={database};")
    result = subprocess.run(["dotnet", str(dll), "migrate"], env=env,
                            capture_output=True, text=True, timeout=130)
    assert (result.returncode == 0) == expected, result.stderr
    return result

def checksum(name):
    sql = (repo / "server" / "Migrations" / name).read_text(encoding="utf-8")
    return hashlib.sha256(sql.encode("utf-8")).hexdigest().upper()

# Two real migrators against a fresh database must both succeed.
first = subprocess.Popen(["dotnet", str(dll), "migrate"], env=base_env)
try:
    migrate()
finally:
    assert first.wait(timeout=130) == 0
migrate()
assert psql("SELECT string_agg(version::text, ',' ORDER BY version) FROM schema_migrations") == "1,2"

# Reconstruct exactly the previously shipped schema, including its checksum.
psql("CREATE DATABASE locus_upgrade")
v1 = (repo / "server" / "Migrations" / "001_initial.sql").read_text(encoding="utf-8")
psql("""
CREATE TABLE schema_migrations (
  version integer PRIMARY KEY, checksum text NOT NULL,
  applied_at timestamptz NOT NULL DEFAULT now()
);
""" + v1 + f"""
INSERT INTO schema_migrations(version, checksum) VALUES (1, '{checksum("001_initial.sql")}');
INSERT INTO players(id,display_name,normalized_name)
VALUES ('aaaaaaaa-0000-0000-0000-000000000001','Existing','existing');
""", "locus_upgrade")
migrate("locus_upgrade")
migrate("locus_upgrade")
assert psql("SELECT display_name FROM players", "locus_upgrade") == "Existing"
assert psql("SELECT count(*) FROM schema_migrations", "locus_upgrade") == "2"

# Reject altered checksums, missing prefixes, unknown versions.
for damage, restore in [
    ("UPDATE schema_migrations SET checksum='tampered' WHERE version=1",
     f"UPDATE schema_migrations SET checksum='{checksum('001_initial.sql')}' WHERE version=1"),
    ("DELETE FROM schema_migrations WHERE version=1",
     f"INSERT INTO schema_migrations(version,checksum) VALUES (1,'{checksum('001_initial.sql')}')"),
    ("INSERT INTO schema_migrations(version,checksum) VALUES (3,'unknown')",
     "DELETE FROM schema_migrations WHERE version=3"),
]:
    psql(damage, "locus_upgrade")
    migrate("locus_upgrade", expected=False)
    psql(restore, "locus_upgrade")
    migrate("locus_upgrade")

# Identity mapping must not collapse distinct case-sensitive subjects or issuers.
db = "locus_upgrade"
p1 = "aaaaaaaa-0000-0000-0000-000000000001"
p2 = "aaaaaaaa-0000-0000-0000-000000000002"
psql(f"INSERT INTO players(id,display_name,normalized_name) VALUES ('{p2}','Other','other')", db)
def bind(issuer, subject, player, expected=True):
    psql(f"INSERT INTO player_identities(issuer,subject,player_id) VALUES ('{issuer}','{subject}','{player}')", db, expected)
bind("https://issuer.example", "Subject", p1)
bind("https://issuer.example", "subject", p2)
bind("https://issuer.example", "Subject", p2, False)
bind("https://issuer.example", "OtherSubject", p1, False)
bind("https://second.example", "Subject", p2)
bind("https://third.example", "Subject", "aaaaaaaa-0000-0000-0000-000000000003", False)
bind("https://third.example", "", p1, False)
assert psql("SELECT count(*) FROM player_identities", db) == "3"
print("Migration and identity constraints passed.")
