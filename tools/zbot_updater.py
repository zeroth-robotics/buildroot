#!/usr/bin/env python3

import asyncio
import websockets
import json
import os
import http.client
import argparse
import socket
import time
import requests
from tqdm import tqdm
from termcolor import colored


def send_command(host, port, command):
    """Send a command to the TCP server and return the response."""
    try:
        with socket.create_connection((host, port), timeout=1) as sock:
            sock.sendall(command.encode() + b"\n")
            response = sock.recv(1024).decode().strip()
            return response 

    except Exception:
        return None


async def check_websocket(host, port):
    """Attempt to connect to the WebSocket server with a timeout."""
    try:
        return await asyncio.wait_for(websockets.connect(f"ws://{host}:{port}/ws"), timeout=2)
    except (asyncio.TimeoutError, websockets.exceptions.ConnectionClosed, OSError):
        return False



def wait_for_reboot(host, port, timeout=120):
    """Wait for the device to reboot and start the SWUpdate WebSocket server."""
    print(colored("waiting for zbot to reboot...", "yellow"))
    start_time = time.time()

    while time.time() - start_time < timeout:
        try:
            if asyncio.run(check_websocket(host, port)):  # Fix: Proper async handling
                print(colored("zbot is back online", "green"))
                return True
        except RuntimeError:
            print(colored("Async event loop error. Retrying...", "red"))
            time.sleep(1)
            continue  # Keep retrying

        time.sleep(1)  # Retry every second

    print(colored("Could not connect to zbot: timeout.", "red"))
    return False



def upload_file_http(file_path, host, port=10000, endpoint="/upload"):
    """Upload SWU file using HTTP POST."""
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
            print("\033[K", end="\r")  # Clear progress bar

            response = conn.getresponse()
            print(f"Response: {response.status} {response.reason}")
            #print(response.read().decode())
            conn.close()
    except Exception as e:
        print(f"Error uploading file: {e}")


async def keep_alive(websocket, interval=10):
    """Send keep-alive pings to WebSocket server."""
    while True:
        try:
            await websocket.ping()
            await asyncio.sleep(interval)
        except websockets.ConnectionClosed:
            print("WebSocket connection closed during keep-alive.")
            break


async def connect(host, file_path):
    """Connect to SWUpdate WebSocket and monitor update process."""
    ws_url = f"ws://{host}:10000/ws"
    progress_bar = None

    try:
        async with websockets.connect(ws_url) as websocket:
            print(colored("connected to update service", "green"))
            asyncio.create_task(keep_alive(websocket))

            upload_file_http(file_path, host)

            while True:
                try:
                    message = await websocket.recv()
                    data = json.loads(message)

                    if data.get("type") == "step":
                        step = int(data.get("step", 0))
                        number = int(data.get("number", 1))
                        percent = int(data.get("percent", 0))
                        name = data.get("name", "Unknown")

                        if progress_bar is None:
                            progress_bar = tqdm(
                                total=100,
                                desc=f"updating: {name}",
                                ncols=80,
                                dynamic_ncols=True,
                                bar_format="{l_bar}{bar}| {n_fmt}/{total_fmt}%",
                                leave=False,
                            )

                        progress_bar.n = percent
                        progress_bar.refresh()

                        if percent == 100:
                            progress_bar.close()
                            progress_bar = None
                            print("\033[K", end="\r")  # Clear progress bar

                    if data.get("type") == "status":
                        status = data.get("status")
                        if status == "DONE":
                            print(colored("Update complete!", "green"))
                            exit(0)
                        elif status == "SUCCESS":
                            print(colored("Successfully completed.", "green"))

                except json.JSONDecodeError:
                    pass
                except websockets.ConnectionClosed:
                    print(colored("Update service connection closed.", "red"))
                    break
    except:
        print(colored(f"zbot not found @ {host}", "red"))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="zbot_updater: a tool for updating zbot firmware.",
        formatter_class=argparse.RawTextHelpFormatter
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    update_parser = subparsers.add_parser("update", help="Upload and apply an SWU file to a zbot device.")
    update_parser.add_argument("host", help="The zbot device host (e.g., 192.168.42.1).")
    update_parser.add_argument("swu_file", help="The SWU package to upload and apply.")

    args = parser.parse_args()

    if args.command == "update":
        host = args.host
        swu_file = args.swu_file

        ota_response = send_command(host, 10000, "ota")

        if ota_response == "zbot":
            print(colored("zbot detected", "grey"))
            print(colored("rebooting...", "grey"))
            if not wait_for_reboot(host, 10000):
                print(colored("Timeout waiting for reboot. Aborting update.", "red"))
                exit(1)

        asyncio.run(connect(host, swu_file))

