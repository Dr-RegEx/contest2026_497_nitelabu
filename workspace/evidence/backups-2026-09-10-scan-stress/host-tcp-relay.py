"""Explicitly bound Windows TCP relay to the WSL SCP fixture."""

import argparse
import asyncio


async def main(args):
    async def handle(reader, writer):
        peer_writer = None
        try:
            peer_reader, peer_writer = await asyncio.wait_for(
                asyncio.open_connection(args.target, args.target_port), 10)

            async def copy(source, destination):
                while data := await source.read(65536):
                    destination.write(data)
                    await destination.drain()
                if destination.can_write_eof():
                    destination.write_eof()

            await asyncio.wait_for(asyncio.gather(copy(reader, peer_writer),
                                                 copy(peer_reader, writer)), 600)
        except (OSError, asyncio.TimeoutError):
            pass
        finally:
            for stream in (writer, peer_writer):
                if stream is not None:
                    stream.close()
                    try:
                        await stream.wait_closed()
                    except OSError:
                        pass

    server = await asyncio.start_server(handle, args.bind, args.port)
    print(f"TCP relay listening on {args.bind}:{args.port}", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", required=True)
    parser.add_argument("--port", type=int, default=22)
    parser.add_argument("--target", required=True)
    parser.add_argument("--target-port", type=int, default=2222)
    asyncio.run(main(parser.parse_args()))
