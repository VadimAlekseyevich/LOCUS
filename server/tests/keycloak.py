"""Browser-style authorization code/PKCE against the actual imported Keycloak realm."""
import base64
import hashlib
from html.parser import HTMLParser
import http.cookiejar
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

dll = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
issuer = "https://127.0.0.1:5444/realms/locus"
api = "http://127.0.0.1:5080"
client = "locus-desktop"
callback = "http://127.0.0.1:39193"
subject = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"

if len(sys.argv) == 4 and sys.argv[3] == "prepare":
    root.mkdir(parents=True, exist_ok=True)
    realm = json.loads((Path(__file__).resolve().parents[2] /
                        "infra/identity/locus-realm.json").read_text())
    password = secrets.token_urlsafe(32)
    realm["users"] = [{"id": subject, "username": "ci-player", "enabled": True,
                       "emailVerified": True, "firstName": "CI", "lastName": "Player",
                       "email": "ci@example.test",
                       "credentials": [{"type": "password", "value": password, "temporary": False}]}]
    (root / "realm.json").write_text(json.dumps(realm))
    (root / "password").write_text(password)
    sys.exit(0)

def get_json(url):
    with urllib.request.urlopen(url, timeout=5) as r:
        return json.load(r)

for _ in range(120):
    try:
        metadata = get_json(issuer + "/.well-known/openid-configuration")
        break
    except (OSError, urllib.error.URLError):
        time.sleep(1)
else:
    raise RuntimeError("Keycloak discovery unavailable")
assert metadata["issuer"] == issuer
assert "S256" in metadata["code_challenge_methods_supported"]

env = dict(os.environ, Auth__Issuer=issuer, Auth__Audience="https://api.locus.test")
def provision(sub, name, success=True):
    r = subprocess.run(["dotnet", str(dll), "provision-player", sub, name],
                       env=env, capture_output=True, text=True, timeout=40)
    assert (r.returncode == 0) == success, r.stderr
    return str(uuid.UUID(r.stdout.strip())) if success else None

player = provision(subject, "KeycloakCI")
provision(subject, "DuplicateSubject", False)
provision(str(uuid.uuid4()), "keycloakci", False)
provision("bad subject", "InvalidSubject", False)
# Duplicate subject insertion must roll back the new player.
check = subprocess.run(["psql", "-X", "-qAt", "-v", "ON_ERROR_STOP=1",
                        "-h", "127.0.0.1", "-U", "locus", "-d", "locus",
                        "-c", "SELECT count(*) FROM players WHERE normalized_name='duplicatesubject'"],
                       capture_output=True, text=True, check=True, timeout=15)
assert check.stdout.strip() == "0"

class StopAtCallback(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlsplit(newurl).netloc == urllib.parse.urlsplit(callback).netloc:
            return None
        return super().redirect_request(req, fp, code, msg, headers, newurl)

class LoginForm(HTMLParser):
    action = None
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "form" and attrs.get("id") == "kc-form-login":
            self.action = attrs.get("action")

def form(values):
    return urllib.parse.urlencode(values).encode()

def b64(raw):
    return base64.urlsafe_b64encode(raw).rstrip(b"=").decode()

def authorize(challenge=True):
    browser = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()),
                                          StopAtCallback())
    verifier = secrets.token_urlsafe(48)
    state = secrets.token_urlsafe(24)
    params = dict(client_id=client, redirect_uri=callback, response_type="code",
                  scope="openid locus.player", state=state, nonce=secrets.token_urlsafe(24))
    if challenge:
        params.update(code_challenge=b64(hashlib.sha256(verifier.encode()).digest()),
                      code_challenge_method="S256")
    url = metadata["authorization_endpoint"] + "?" + urllib.parse.urlencode(params)
    try:
        with browser.open(url, timeout=15) as r:
            parser = LoginForm()
            parser.feed(r.read().decode())
            action = urllib.parse.urljoin(url, parser.action or "")
            assert parser.action and action.startswith(issuer + "/")
        assert challenge, "Authorization without mandatory PKCE reached login"
        browser.open(action, form(dict(username="ci-player", password=(root / "password").read_text())), timeout=15)
        raise AssertionError("Expected authorization callback")
    except urllib.error.HTTPError as e:
        if not challenge:
            assert e.code in (302, 400), e.code
            if e.code == 302:
                assert "error" in urllib.parse.parse_qs(urllib.parse.urlsplit(e.headers["Location"]).query)
            return None
        assert e.code == 302, e.code
        location = e.headers["Location"]
        assert location.startswith(callback + "?") or location.startswith(callback + "/?")
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(location).query)
        assert query["state"] == [state] and "code" in query
        return query["code"][0], verifier

def token_request(values, expected=200):
    try:
        response = urllib.request.urlopen(metadata["token_endpoint"], form(dict(client_id=client, **values)), timeout=15)
    except urllib.error.HTTPError as e:
        response = e
    with response:
        assert response.code == expected, response.code
        return json.load(response)

def exchange(code, verifier, expected=200):
    return token_request(dict(grant_type="authorization_code", code=code,
                              redirect_uri=callback, code_verifier=verifier), expected)

authorize(False)
code, verifier = authorize()
exchange(code, secrets.token_urlsafe(48), 400)
code, verifier = authorize()
tokens = exchange(code, verifier)
exchange(code, verifier, 400)
def assert_access_contract(access):
    # Test-only decoding, never a substitute for the API signature validation.
    # Diagnostic messages deliberately contain no token or personal claim values.
    def segment(index):
        value = access.split(".")[index]
        return json.loads(base64.urlsafe_b64decode(value + "=" * (-len(value) % 4)))
    header, claims = segment(0), segment(1)
    assert header.get("typ", "").lower() == "at+jwt" and header.get("alg") == "RS256", "Unexpected token type or algorithm"
    assert claims.get("iss") == issuer, "Issuer claim mismatch"
    assert claims.get("sub") == subject, "Missing or incorrect immutable subject mapping"
    audiences = claims.get("aud", [])
    if isinstance(audiences, str):
        audiences = [audiences]
    assert "https://api.locus.test" in audiences, "Missing LOCUS API audience"
    assert "locus.player" in claims.get("scope", "").split(), "Missing player scope"
    assert "act" not in claims, "Unexpected delegated identity"

assert_access_contract(tokens["access_token"])

def request(path, bearer=None, expected=200):
    req = urllib.request.Request(api + path)
    if bearer:
        req.add_header("Authorization", "Bearer " + bearer)
    try:
        response = urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as e:
        response = e
    with response:
        assert response.code == expected, (path, response.code, expected)
        return json.load(response) if expected == 200 else None

with (root / "api.log").open("w+") as log:
    process = subprocess.Popen(["dotnet", str(dll), "--urls", api], env=env,
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
        assert request("/api/v1/me", tokens["access_token"])["id"] == player
        request("/api/v1/me", tokens["id_token"], 401)
        refreshed = token_request(dict(grant_type="refresh_token", refresh_token=tokens["refresh_token"]))
        assert_access_contract(refreshed["access_token"])
        assert request("/api/v1/me", refreshed["access_token"])["id"] == player
        assert refreshed["refresh_token"] != tokens["refresh_token"]
        token_request(dict(grant_type="refresh_token", refresh_token=tokens["refresh_token"]), 400)
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.seek(0)
        print(log.read())
print("Real Keycloak browser code, PKCE, player binding and refresh rotation passed.")
