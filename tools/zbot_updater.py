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
import requests

def print_status(message):
    try:
        #print(colored(f"{message}: ", "grey"))
        data = json.loads(message)
        message_type = data.get("type", "unknown")

        if message_type == "info":
            text = data.get("source", "")
            if "{" in text:
                return
            print(colored(f"{message_type}: ", "grey") + colored(text, "white"))

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


async def keep_alive(websocket, interval=10):
    while True:
        try:
            await websocket.ping()
            await asyncio.sleep(interval)
        except websockets.ConnectionClosed:
            print("WebSocket connection closed during keep-alive.")
            break


async def connect(host, file_path):
    #We send a single request to help swupdate initialize
    url = f"http://{host}:10000/"

    try:
        response = requests.get(url)
    except:
        print(f"Failed to connect to zbot @ {host}")
        exit(1)

    ws_url = f"ws://{host}:10000/ws"
    progress_bar = None

    async with websockets.connect(ws_url) as websocket:
        print("connected to zbot update service")

        asyncio.create_task(keep_alive(websocket))

        upload_file_http(file_path, host)

        while True:
            try:
                message = await websocket.recv()
                print_status(message)
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
                        print(colored("Writing image...", "blue"))

            except json.JSONDecodeError:
                pass
            except websockets.ConnectionClosed:
                print("Update service connection closed.")
                break

def main():
    parser = argparse.ArgumentParser(
        description=(
            "zbot_updater: a tool for updating zbot firmware.\n\n"
            "Usage examples:\n"
            "  zbot_updater.py update 192.168.42.1 /path/to/firmware.swu\n"
        ),
        formatter_class=argparse.RawTextHelpFormatter
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    # Update command
    update_parser = subparsers.add_parser(
        "update", help="Upload and apply an SWU file to a zbot device."
    )
    update_parser.add_argument("host", help="The zbot device host (e.g., 192.168.42.1).")
    update_parser.add_argument("swu_file", help="The SWU package to upload and apply.")


    try:
        args = parser.parse_args()

        if args.command == "update":
            asyncio.run(connect(args.host, args.swu_file))

    except SystemExit as e:
        if e.code != 0: 
            exit(e.code)

if __name__ == "__main__":
    main()
