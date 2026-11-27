"""Windows-only local TLS smoke. No external network or production credentials."""
import http.server
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading

exe = Path(sys.argv[1]).resolve()
openssl = sys.argv[2]
seen = []
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, body=b"{}", headers=()):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        for key, value in headers:
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        seen.append(self.path)
        if self.path == "/ok":
            self.reply(200 if self.headers.get("Authorization") == "Bearer fixture.token" else 403)
        elif self.path == "/redirect":
            self.reply(302, headers=[("Location", "/must-not-follow")])
        elif self.path == "/large":
            self.reply(200, b"x" * 1000)
        elif self.path == "/encoded":
            self.reply(200, headers=[("Content-Encoding", "gzip")])
        else:
            self.reply(403)

    def do_POST(self):
        seen.append(self.path)
        body = self.rfile.read(int(self.headers["Content-Length"]))
        self.reply(200 if self.path == "/form" and body == b"code=a%2Bb%26c" and
                   self.headers.get("Content-Type") == "application/x-www-form-urlencoded" and
                   not self.headers.get("Authorization") else 400)

def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=30).stdout

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    servers = []
    threads = []
    fingerprint = None
    try:
        for label in ("trusted", "untrusted"):
            cert, key = root / (label + ".crt"), root / (label + ".key")
            run(openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                "-subj", "/CN=LOCUS local transport " + label,
                "-addext", "subjectAltName=IP:127.0.0.1",
                "-addext", "basicConstraints=critical,CA:TRUE",
                "-keyout", str(key), "-out", str(cert))
            if label == "trusted":
                fingerprint = run(openssl, "x509", "-in", str(cert), "-noout",
                                  "-fingerprint", "-sha1").strip().split("=")[1].replace(":", "")
                run("certutil", "-user", "-addstore", "Root", str(cert))
            server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ctx.minimum_version = ssl.TLSVersion.TLSv1_2
            ctx.load_cert_chain(cert, key)
            server.socket = ctx.wrap_socket(server.socket, server_side=True)
            servers.append(server)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            threads.append(thread)
        subprocess.run([str(exe), *[f"https://127.0.0.1:{s.server_port}" for s in servers]],
                       check=True, timeout=90)
        assert seen == ["/ok", "/form", "/redirect", "/large", "/encoded", "/denied"], seen
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()
        for thread in threads:
            thread.join(timeout=5)
        if fingerprint:
            run("certutil", "-user", "-delstore", "Root", fingerprint)
print("WinHTTP TLS, redirect and response limits passed.")
