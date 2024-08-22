import numpy as np
import xarray as xr
import socket

def split(data):
    header = data[:24]
    rest = data[24:].reshape(-1, 13)
    return header, rest

def read_iwv_tmp(data):
    header, rest = split(data)
    time = np.datetime64("2001-01-01") + (rest[:, :4].view("<u4").ravel() * np.timedelta64(1000000000, "ns"))
    iwv = rest[:,5:9].view("<f4").ravel()
    return xr.Dataset({
        "time": (("time",), time),
        "iwv": (("time",), iwv, {"units": "kg/m2"}),
    })

def udp_push(msg):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect(("192.168.1.255", 5001))
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.send(msg.encode("utf-8"))

def forward_once(filename):
    with open(filename, "rb") as infile:
        data = infile.read()
    data = np.frombuffer(data, dtype="u1")
    data = read_iwv_tmp(data)
    msg = f"KV,{data.time.values[-1]:s},{data.iwv.values[-1]}"
    print(msg)
    udp_push(msg)

def main():
    import sys
    import time
    filename = sys.argv[1]

    while True:
        try:
            forward_once(filename)
        except IOError as e:
            print("IO Error", e)
        time.sleep(60)


if __name__ == "__main__":
    exit(main())
