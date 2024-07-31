#include <cstring>
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <conio.h>
#include <exception>
#include <string>
#include <functional>
#include "Util.h"

#pragma comment(lib, "Ws2_32.lib")

ThreadPool service_threads;
Result results;

sockaddr_in target; 
std::string target_IP; // Target IP address
int ports; // Number of ports to scan
const int BUFFER_SIZE = 2048;
std::atomic <bool> scanning = false;

std::unordered_map <int, std::string> service_port;
std::unordered_map <std::string, std::string> service_request;
std::mutex service_mutex;

void get_service(int port, SOCKET &sock)
{
	char buffer[BUFFER_SIZE];
	std::string service = "", request = "GET / HTTP / 1.1\r\nHost: " + ::target_IP + "\r\n\r\n";
	std::vector <std::string> requests =
	{
		"GET / HTTP / 1.1\r\nHost: " + ::target_IP + "\r\n\r\n",
		"EHLO example.com\r\n"
	};

	// Set timeout for recv operation
	//struct timeval timeout;
	//timeout.tv_sec = 5;  // Timeout in seconds
	//timeout.tv_usec = 0; // Not using microseconds in this case
	//setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	for (int i = 0; i < requests.size(); i++)
	{
		// Send request
		int result = send(sock, requests[i].c_str(), static_cast<int>(requests[i].length()), 0);

		// Case: Unknown
		if (result == SOCKET_ERROR)
		{
			//sync_print("Banner: " + std::string(buffer, 0, result) + "\n");
			::results.updateServices(port, "Unknown");
			continue;
		}

		// Receive response from target
		result = recv(sock, buffer, BUFFER_SIZE, 0);

		// Case: Unknown
		if (result == SOCKET_ERROR)
		{
			::results.updateServices(port, "Unknown");
			//continue;
			//sync_print("Banner: " + std::string(buffer, 0, result) + "\n");	
		}
		else
		{
			::results.updateBanners(port, std::string(buffer, 0, result));
			service = detService(std::string(buffer, 0, result));
			if (service != "Unknown")
			{
				::results.updateServices(port, service);
				break;
			}
			::results.updateServices(port, service);
		}
	}
	// Update banners
	::results.updateServices(port, service);
	closesocket(sock);
}

// Updates progress 
void check_progress()
{
	char input;
	while (::scanning)
	{
		// Check for up arrow key sequence
		if (_kbhit() != 0) 
		{
			input = _getch();
			if (input == 'p')
			{
				std::cout << "\033[93m[!] \033[97m"; // Yellow
				std::cout << "Progress: " <<
					100 * (double(::results.getSize()) / ::ports) << "%" << std::endl;
			}
		}
	}
}

void scan_range(int start, int end)
{
		int result = 0;
		char buffer[::BUFFER_SIZE] = { 0 };

		for (; start <= end; start++)
		{
			// Create a socket, AF_INET specifies this is to be used with IPv4 addresses, SOCK_STREAM specifies a two way connection
			// Using tcp(sockstream connection), IPPROTO_TCP specifies that TCP protocol should be used for this socket 
			SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			// Validate Socket Connection
			if (sock == INVALID_SOCKET)
			{
				std::cout << "\033[91m[-] \033[97m"; // Red
				std::cerr << ("Socket Creation Failure @ Port: " + start) << std::endl;
				continue;
			}

			// Specify port i, htons ensure proper byte order as sin_port accepts network byte order
			::target.sin_port = htons(start);

			// Attempt socket connection
			result = connect(sock, reinterpret_cast<SOCKADDR*>(&::target), sizeof(::target));

			if (result == SOCKET_ERROR) // Port is closed
				::results.updateClosedPorts(start);
			else // Port is open
			{
				::results.updateOpenPorts(start);
				service_threads.pushTask(get_service, start, sock); // Add task to thread pool
			}
		}
}

void scan_target()
{

	std::vector <std::thread> threads;
	const int THREAD_COUNT = 10;
	int start, end;

	// Check for user input
	::scanning = true;
	std::thread check(check_progress);

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		// Calculate port range for threads
		start = i * (::ports / THREAD_COUNT) + 1;
		end = (i + 1) * (::ports / THREAD_COUNT);

		// Run threads
		threads.push_back(std::thread(scan_range, start, end));
	}

	// Wait for all port scanning threads to complete
	for (auto& thread : threads)
	{
		thread.join();
	}

	//while (service_threads.busy()) {}; // Wait till threads are done working
	::scanning = false;
	check.join(); // Wait for user input thread to close
}

int main()
{
	try 
	{
		::target.sin_family = AF_INET; // Set target to IPv4 address family
		WSADATA wsaData; // Hold winsocket details
		int result = WSAStartup(MAKEWORD(2, 2), &wsaData); // Initialize Winsock 2.2 // MAKEWORD concatenates to 2.2

		// Validate Winsock implementation
		if (result != 0)
			throw std::runtime_error("\033[91m[-] WSAStartup Failed: " + WSAGetLastError());
			
		Sleep(200);
		// Get target IP address
		std::cout << "\033[94m[*] \033[97m"; // Blue
		std::cout << "Enter Target: ";
		std::cin >> ::target_IP;
		std::cout << "\033[94m[*] \033[97m"; // Blue
		std::cout << "Enter Ports to Scan: ";
		std::cin >> ::ports;

		// Scans target
		std::cout << "\033[94m[*] \033[97m"; // Green
		std::cout << "Scanning Target ..." << std::endl;

		// Converts IP address to into a form that is readable for network operations
		inet_pton(AF_INET, ::target_IP.c_str(), &::target.sin_addr);
		scan_target(); // Begins scanning the target

		WSACleanup(); // Clean Up winsock
	}
	catch (std::exception& e)
	{
		std::cerr << e.what();
		WSACleanup();
		return 1;
	}
	::results.complete = true;
	::results.print(); // Print scan results
	::results.menu();
	return 0;
}
