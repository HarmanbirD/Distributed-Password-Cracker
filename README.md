# Reliable UDP Protocol Simulation

A simulation of a reliable UDP-based network protocol developed for BCIT's Network Security course. This protocol maintains data integrity in lossy network conditions and supports both IPv4 and IPv6.

## Features

- Client packet reading and transmission to the server.
- Window-based packet transmission without waiting for ACKs up to `window_size.`
- Packet buffering if the transmission window is full.
- Proxy simulation of packet delay, drop, or corruption.
- Dynamic adjustment of drop, delay, or corruption rates.
- Cumulative acknowledgment by the client.
- Provides a Python3 GUI for real-time data flow visualization and performance assessment.

## Installation

Clone the repository and build the server, client, and proxy modules, as well as run the GUI with the following commands:

### Clone the repository
```sh
git clone https://github.com/BScACS-T2/Reliable-UDP.git
cd Reliable-UDP
```
### Build the Server
```sh
mkdir -p server/cmake-build-debug && cd server/cmake-build-debug
cmake ..
make
cd ../../
```

### Build the Client
```sh
mkdir -p client/cmake-build-debug && cd client/cmake-build-debug
cmake ..
make
cd ../../
```
### Build the Proxy
```sh
mkdir -p proxy/cmake-build-debug && cd proxy/cmake-build-debug
cmake ..
make
cd ../../
```

## Usage
Run the server, client, and proxy using the following commands. Remember to replace the placeholder IP addresses and ports with the actual values you're using:

### Run the Server
```sh
./server -C <Client IP> -c <Client port> -S <Server IP> -s <Server port>
```
### Run the Client
```sh
./client -C <Client IP> -c <Client port> -S <Server IP> -s <Server port> -w <window size>
```
### Run the Proxy
```sh
./proxy -C <Client IP> -c <Client port> -S <Server IP> -s <Server port> -P <Proxy IP> \
-D <Client drop rate> -d <Server drop rate> -L <Client delay rate> -l <Server delay rate> -E <Corruption rate>
```
### Run the GUI
```sh
python3 main.py
```
## Dynamic Protocol Features
- Window-based transmission is managed with the -w flag on the client.
- Proxy lossiness can be simulated with the -D, -d, -L, -l, and -E flags.
