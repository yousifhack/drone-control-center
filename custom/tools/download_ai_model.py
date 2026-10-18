"""Download the evaluated candidate model to an explicit path and verify SHA-256."""

import argparse
import hashlib
import os
import tempfile
import urllib.request
from pathlib import Path

URL = (
    "https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/"
    "91cad51170dc346986eccefdc2dd33a9da36ead9/qwen2.5-1.5b-instruct-q4_k_m.gguf"
)
SHA256 = "6a1a2eb6d15622bf3c96857206351ba97e1af16c30d7a74ee38970e434e9407e"


def download(destination):
    destination = destination.expanduser().resolve()
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix="model-download-", dir=destination.parent)
    try:
        digest = hashlib.sha256()
        with os.fdopen(fd, "wb") as output, urllib.request.urlopen(URL, timeout=90) as response:
            while chunk := response.read(1024 * 1024):
                digest.update(chunk)
                output.write(chunk)
        if digest.hexdigest() != SHA256:
            raise ValueError("Model checksum mismatch; the download will not be installed")
        if destination.exists():
            raise FileExistsError("Destination appeared during download")
        os.replace(temporary, destination)
        print(f"Verified model saved: {destination}")
        print(
            "This is an evaluated candidate, not an approved autonomous-control model. See the evaluation report."
        )
    finally:
        Path(temporary).unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    download(parser.parse_args().output)
