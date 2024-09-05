# Winsock Port Scanner

## Description

- This a port scanner implemented through winsock raw sockets.
- This project was done to gain knowledge and experience in using low level windows sockets and security concepts.
- This repository was made public as part of an effort to document more of my work.

## Features

- The scanner utilizes an underlying threadpool to concurrently scan the supplied ports.
- Upon detection of an open port, the scanner will send an HTTP GET request to the target and parse the response banner to determine the service running.
- While scanning, the progress can be monitored by pressing the 'p' key.
- The port number and service of the open ports will be displayed post scan.

## Usage

- Ensure you have a windows C++ compatable compiler installed.
- The scanner will need to be ran on a windows machine.
- When ran, the scanner will prompt the user to enter the target IP address and ports.
