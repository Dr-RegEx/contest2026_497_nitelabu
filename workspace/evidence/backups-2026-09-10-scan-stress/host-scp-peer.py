"""Dedicated password-authenticated SCP fixture; no shell or OS account."""

import argparse
import asyncio
import hmac
import json
import os
from pathlib import Path
import secrets

import asyncssh


def initialize(private):
    private.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(private, 0o700)
    config = private / "credentials.json"
    if not config.exists():
        with config.open("x") as stream:
            os.chmod(config, 0o600)
            json.dump({"username": "xts", "password": secrets.token_urlsafe(24)}, stream)
    key = private / "host-rsa"
    if not key.exists():
        with key.open("xb") as stream:
            os.chmod(key, 0o600)
            stream.write(asyncssh.generate_private_key("ssh-rsa", key_size=2048)
                         .export_private_key())
    return json.loads(config.read_text()), key


async def listen(root, private, bind, port):
    credentials, key = initialize(private)
    root.mkdir(parents=True, exist_ok=True)

    class Server(asyncssh.SSHServer):
        def begin_auth(self, username):
            return True

        def password_auth_supported(self):
            return True

        def validate_password(self, username, password):
            return (hmac.compare_digest(username, credentials["username"])
                    and hmac.compare_digest(password, credentials["password"]))

    return await asyncssh.listen(
        bind, port, server_factory=Server, server_host_keys=[str(key)],
        sftp_factory=lambda channel: asyncssh.SFTPServer(channel, chroot=str(root)),
        allow_scp=True, login_timeout=30)


async def main(args):
    server = await listen(args.root.resolve(), args.private.resolve(), args.bind, args.port)
    print(json.dumps({"status": "SCP_PEER_LISTENING", "bind": args.bind,
                      "port": server.get_port(), "root": str(args.root.resolve())}),
          flush=True)
    async with server:
        await server.wait_closed()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--private", type=Path, required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2222)
    asyncio.run(main(parser.parse_args()))
