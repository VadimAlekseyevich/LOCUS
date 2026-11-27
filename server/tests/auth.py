"""Exercise the real JWT bearer handler against a disposable TLS discovery issuer.
The signing keys and issuer exist only in this test; no production auth bypass.
"""
import base64
from contextlib import contextmanager
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request

dll = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
issuer = "https://127.0.0.1:5443"
base = "http://127.0.0.1:5080"
audience = "https://api.locus.test"
key = root / "signing.key"
other = root / "other.key"

def openssl(*args, data=None):
    return subprocess.run(["openssl", *map(str, args)], input=data,
                          capture_output=True, check=True, timeout=15).stdout

def b64(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")

for path in (key, other):
    openssl("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", path)
modulus = openssl("rsa", "-in", key, "-noout", "-modulus").decode().strip().split("=", 1)[1]
jwks = {"keys": [{"kty": "RSA", "kid": "fixture", "use": "sig", "alg": "RS256",
                  "n": b64(bytes.fromhex(modulus)), "e": "AQAB"}]}
metadata = {"issuer": issuer, "jwks_uri": issuer + "/jwks",
            "authorization_endpoint": issuer + "/authorize", "token_endpoint": issuer + "/token",
            "id_token_signing_alg_values_supported": ["RS256"],
            "response_types_supported": ["code"], "subject_types_supported": ["public"]}

class Discovery(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        value = {"/.well-known/openid-configuration": metadata, "/jwks": jwks}.get(self.path)
        if value is None:
            self.send_error(404)
            return
        data = json.dumps(value).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

def token(subject="alpha", typ="at+jwt", signing=key, **claims):
    now = int(time.time())
    payload = dict(iss=issuer, aud=audience, sub=subject, scope="locus.player",
                   iat=now, nbf=now-1, exp=now+300, client_id="fixture", jti="fixture")
    payload.update(claims)
    encoded = ".".join(b64(json.dumps(x, separators=(",", ":")).encode()) for x in
                       ({"alg": "RS256", "kid": "fixture", "typ": typ}, payload))
    return encoded + "." + b64(openssl("dgst", "-sha256", "-sign", signing, data=encoded.encode()))

def sql(statement):
    subprocess.run(["psql", "-X", "-q", "-v", "ON_ERROR_STOP=1", "-h", "127.0.0.1",
                    "-U", "locus", "-d", "locus", "-c", statement], check=True, timeout=15)

def request(path="/api/v1/me", bearer=None, expected=200):
    req = urllib.request.Request(base + path)
    if bearer is not None:
        req.add_header("Authorization", "Bearer " + bearer)
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        assert response.code == expected, (path, response.code, expected, response.read())
        if path == "/api/v1/me":
            assert response.headers.get("Cache-Control") == "no-store"
        if expected == 401:
            assert response.headers.get("WWW-Authenticate", "").startswith("Bearer")
        return json.load(response) if expected == 200 else None

@contextmanager
def running(env):
    with (root / "api.log").open("w+") as log:
        process = subprocess.Popen(["dotnet", str(dll), "--urls", base], env=env,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(80):
                if process.poll() is not None:
                    raise RuntimeError("API exited")
                try:
                    request("/health/live")
                    break
                except urllib.error.URLError:
                    time.sleep(.25)
            else:
                raise RuntimeError("API did not start")
            yield
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            log.seek(0)
            print(log.read())

alpha = "00000000-0000-0000-0000-000000000001"
beta = "00000000-0000-0000-0000-000000000002"
disabled = "00000000-0000-0000-0000-000000000003"
sql(f"""INSERT INTO player_identities(issuer,subject,player_id) VALUES
 ('{issuer}','alpha','{alpha}'), ('{issuer}','beta','{beta}'),
 ('{issuer}','disabled','{disabled}')""")

server = http.server.ThreadingHTTPServer(("127.0.0.1", 5443), Discovery)
tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
tls.load_cert_chain(root / "tls.crt", root / "tls.key")
server.socket = tls.wrap_socket(server.socket, server_side=True)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    env = dict(os.environ, Auth__Issuer=issuer, Auth__Audience=audience)
    with running(env):
        request(expected=401)
        request(bearer="not-a-token", expected=401)
        valid = token()
        assert request(bearer=valid)["id"] == alpha
        assert request(bearer=token("beta"))["id"] == beta
        assert request(bearer=token(player_id=beta, email="beta@example.test"))["id"] == alpha
        for invalid in (token(signing=other), token(iss=issuer+"/other"),
                        token(aud=audience+"/"), token(typ="JWT"),
                        token(exp=int(time.time())-60),
                        token(nbf=int(time.time())+120)):
            request(bearer=invalid, expected=401)
        for forbidden in (token("unknown"), token("Alpha"), token("disabled"),
                          token(scope="locus.player.other"), token(scope=""), token(subject=""),
                          token(act={"sub": "other"})):
            request(bearer=forbidden, expected=403)
        sql(f"UPDATE players SET active=false WHERE id='{alpha}'")
        try:
            request(bearer=valid, expected=403)
        finally:
            sql(f"UPDATE players SET active=true WHERE id='{alpha}'")
        assert request(bearer=valid)["id"] == alpha
        sql(f"DELETE FROM player_identities WHERE issuer='{issuer}' AND subject='alpha'")
        request(bearer=valid, expected=403)
        request("/api/v1/maps")
    env.pop("Auth__Issuer")
    env.pop("Auth__Audience")
    with running(env):
        request(bearer=token("beta"), expected=401)
        request("/api/v1/maps")
finally:
    server.shutdown()
    server.server_close()
    thread.join(timeout=5)
print("Authentication and active-player authorization passed.")
