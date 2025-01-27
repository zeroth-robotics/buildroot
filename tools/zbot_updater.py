#!/usr/bin/env python3
import asyncio
import websockets
from termcolor import colored
import json
from tqdm import tqdm
import os
import http.client
import argparse
import subprocess
from pathlib import Path

def pretty_print(message):
    try:
        data = json.loads(message)
        message_type = data.get("type", "unknown")

        if message_type == "message":
            text = data.get("text", "")
            if "[network_initializer]" in text:
                return

            print(colored(f"{message_type}: ", "grey") + colored(text, "green"))
    except json.JSONDecodeError:
        pass

def upload_file_http(file_path, host, port=10000, endpoint="/upload"):
    try:
        file_size = os.path.getsize(file_path)
        boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW"
        headers = {
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Connection": "keep-alive",
        }

        body_start = (
            f"--{boundary}\r\n"
            f"Content-Disposition: form-data; name=\"file\"; filename=\"{os.path.basename(file_path)}\"\r\n"
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode()

        body_end = f"\r\n--{boundary}--\r\n".encode()
        total_size = len(body_start) + file_size + len(body_end)

        with open(file_path, "rb") as file, tqdm(total=file_size, unit="B", unit_scale=True, desc="uploading") as bar:
            conn = http.client.HTTPConnection(host, port)
            conn.putrequest("POST", endpoint)
            conn.putheader("Content-Length", str(total_size))
            for key, value in headers.items():
                conn.putheader(key, value)
            conn.endheaders()

            conn.send(body_start)
            while chunk := file.read(1024):
                conn.send(chunk)
                bar.update(len(chunk))
            conn.send(body_end)

            bar.close()
            print("\033[K", end="\r")  # Clear the progress bar line

            response = conn.getresponse()
            print(f"Response: {response.status} {response.reason}")
            print(response.read().decode())
            conn.close()
    except Exception as e:
        print(f"Error: {e}")

async def connect(host, file_path):
    url = f"ws://{host}:10000/ws"
    progress_bar = None

    async with websockets.connect(url) as websocket:
        print("connected to zbot update service")
        upload_file_http(file_path, host)

        while True:
            try:
                message = await websocket.recv()
                pretty_print(message)
                data = json.loads(message)

                if data.get("type") == "step":
                    step = int(data.get("step", 0))
                    number = int(data.get("number", 1))
                    percent = int(data.get("percent", 0))
                    name = data.get("name", "Unknown")

                    if progress_bar is None:
                        progress_bar = tqdm(
                            total=100,
                            desc=f"Step {step}/{number}: {name}",
                            ncols=80,
                            dynamic_ncols=True,
                            bar_format="{l_bar}{bar}| {n_fmt}/{total_fmt}%",
                            leave=False,  # Do not leave the progress bar after completion
                        )

                    progress_bar.n = percent
                    progress_bar.refresh()

                    if percent == 100:
                        progress_bar.close()
                        progress_bar = None
                        print("\033[K", end="\r")  # Clear the progress bar line

                if data.get("type") == "status":
                    status = data.get("status")
                    if status == "DONE":
                        exit(0)
                    elif status == "SUCCESS":
                        print(colored("Successfully completed.", "green"))
                    elif status == "START":
                        pass
                    elif status == "RUN":
                        pass
                    else:
                        print(colored(data.get("status"), "red"))

            except json.JSONDecodeError:
                pass
            except websockets.ConnectionClosed:
                print("Update service connection closed.")
                break

def handle_status(status):
    if status == "SUCCESS":
        print(colored("Update completed successfully!", "green"))
        exit(0)
    elif status == "DONE":
        print(colored("All tasks completed.", "blue"))
    else:
        print(colored(f"Status: {status}", "yellow"))

def handle_create_command(artifacts_path):
    print(f"Creating SWU package from artifacts in {artifacts_path}...")

    # Check if the directory exists
    artifacts_dir = Path(artifacts_path)
    if not artifacts_dir.is_dir():
        print(f"Error: {artifacts_path} is not a valid directory.")
        return

    # Check for required files
    required_files = ["sw-description"]
    files_in_dir = {file.name for file in artifacts_dir.iterdir() if file.is_file()}

    if not all(req_file in files_in_dir for req_file in required_files):
        print("Error: Missing required files. Ensure 'sw-description' is present.")
        return

    # Get all filenames, ensuring 'sw-description' comes first
    ordered_files = ["sw-description"] + sorted(
        file for file in files_in_dir if file != "sw-description"
    )

    # Create the SWU package with the same name as the parent directory
    output_file = artifacts_dir.parent / f"zbot_{artifacts_dir.name}.swu"
    try:
        # Change to the artifacts directory
        os.chdir(artifacts_path)

        with subprocess.Popen(
            ["cpio", "-ov", "-H", "crc"],
            stdin=subprocess.PIPE,
            stdout=open(output_file, "wb")
        ) as cpio_process:
            for filename in ordered_files:
                cpio_process.stdin.write(f"{filename}\n".encode())
            cpio_process.stdin.close()
            cpio_process.wait()

        print(f"SWU package created: {output_file}")
    except Exception as e:
        print(f"Error creating SWU package: {e}")

def main():
    parser = argparse.ArgumentParser(
        description=(
            "zbot_updater: A tool for updating zbot firmware and creating SWU firmware packages.\n\n"
            "Usage examples:\n"
            "  zbot_updater.py update 192.168.42.1 /path/to/firmware.swu\n"
            "  zbot_updater.py create /path/to/swu_artifacts\n"
        ),
        formatter_class=argparse.RawTextHelpFormatter  # Preserve newlines in help text
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    # Update command
    update_parser = subparsers.add_parser(
        "update", help="Upload and apply an SWU file to a zbot device."
    )
    update_parser.add_argument("host", help="The zbot device host (e.g., 192.168.42.1).")
    update_parser.add_argument("swu_file", help="The SWU package to upload and apply.")

    # Create command
    create_parser = subparsers.add_parser(
        "create", help="Create an SWU package from artifacts."
    )
    create_parser.add_argument(
        "artifacts_path", help="Path to the directory containing SWU artifacts."
    )

    try:
        args = parser.parse_args()

        if args.command == "update":
            asyncio.run(connect(args.host, args.swu_file))
        elif args.command == "create":
            handle_create_command(args.artifacts_path)
    except SystemExit as e:
        if e.code != 0: 
            exit(e.code)

if __name__ == "__main__":
    main()
