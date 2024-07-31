#ifndef UTIL_H
#define UTIL_H
#include <iostream>
#include <cstdlib>
#include <mutex>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include <thread>
#include <functional>
#include <condition_variable>
#include <utility>
#include <iomanip>


std::mutex print_mutex;
void sync_print(std::string message)
{
	std::lock_guard <std::mutex> lock(print_mutex);
	std::cout << "Thread: " << std::this_thread::get_id() 
		      << " " << message
		      << std::endl;
}

void toLower(std::string& str)
{
	for (int i = 0; i < str.length(); i++)
	{
		str[i] = std::tolower(str[i]);
	}
}

void toUpper(std::string& str)
{
	for (int i = 0; i < str.length(); i++)
	{
		str[i] = std::toupper(str[i]);
	}
}
//std::mutex ser;
std::string detService(std::string banner)
{
	//std::lock_guard<std::mutex> lock(ser);
	std::string foundService;
	size_t pos;

	std::string services[] = 
	{
	"SSH", "FTP", "HTTP", "HTTPS", "SMTP", "TELNET", "IMAP", "POP3",
	"DNS", "MYSQL", "POSTGRESQL", "RDP", "VNC", "SIP", "LDAP", "NTP",
	"SNMP", "SMB", "RPC", "NETBIOS", "NFS", "CIFS", "TFTP", "TELNET"
	};

	toUpper(banner);
	for (int i = 0; i < sizeof(services) / sizeof(services[0]); i++)
	{
		pos = banner.find(services[i]);
		if (pos == std::string::npos)
			continue;
		else
			return services[i];
	}
	return "Unknown";
}


struct Result
{

private:

	std::vector <int> openPorts;
	std::vector <int> closedPorts;

	std::unordered_map <int, std::string > portServices;
	std::unordered_map <int, std::string > banners;

	std::mutex open_mutex;
	std::mutex closed_mutex;
	std::mutex banner_mutex;
	std::mutex service_mutex;

public:
	std::atomic <bool> complete = false;

	void updateOpenPorts(const int port)
	{
		std::lock_guard <std::mutex> guard(open_mutex);

		openPorts.push_back(port);
	}

	void updateClosedPorts(const int port)
	{
		std::lock_guard <std::mutex> guard(closed_mutex);

		closedPorts.push_back(port);
	}

	void updateServices(const int port, const std::string banner)
	{
		std::lock_guard <std::mutex> guard(service_mutex);
		
		portServices[port] = banner;
	}

	void updateBanners(const int port, const std::string banner)
	{
		std::lock_guard <std::mutex> guard(banner_mutex);

		banners[port] = banner;
	}

	const int getSize()
	{
		std::lock_guard <std::mutex> guard(banner_mutex);

		return (openPorts.size() + closedPorts.size());
	}
	
	const void print()
	{
		if (!complete)
			return;
		
		std::cout << std::endl;
		if (openPorts.size() == 0)
		{
			std::cout << "\033[91m[-] \033[97m" // Red
			          << "All Ports Closed/Filtered: " << closedPorts.size() 
				      << std::endl;
			return;
		}

		std::sort(openPorts.begin(), openPorts.end()); // Sort openPorts into ascending order

		// Print Output
		std::cout << "\033[94m[*] \033[97m" // Blue
			<< std::left << std::setw(15) << "Port"
			<< std::setw(10) << "Status"
			<< std::setw(10) << "Service"
			<< std::endl
			<< std::setfill('-') << std::setw(40) << ""
			<< std::setfill(' ') << std::endl;


		for (int i = 0; i < openPorts.size(); i++)
		{
			std::cout << "\033[92m[+] \033[97m" // Green
				      << std::left << std::setw(15) << openPorts[i]
				      << std::setw(10) << "Open"
				      << std::setw(10) << portServices[openPorts[i]]
			          << std::endl;	
		}

		std::cout << std::endl
			      << "\033[92m[+] \033[97m" // Green
			      << "Total Open Ports: " << openPorts.size()
			      << std::endl;

		std::cout << "\033[91m[-] \033[97m" // Red
		          << "Total Closed/Filtered Ports: " << closedPorts.size() 
			      << std::endl;
	}

	//Menu for viewing banners
	const void menu()
	{
		std::string input;

		if (!complete)
			return;
		 
		do {
			std::cout << "\033[94m[*] \033[97m"; // Blue
			std::cout << "View Banner(x to exit): ";
			std::cin >> input;

			if (input == "x" || input == "X")
				return;

			try
			{
				int port = std::stoi(input);

				std::cout << "\033[94m[*] \033[97m"; // Green
				std::cout << "[" << port << "] Server Response: " << banners[port]
					<< std::endl;
			}
			catch (std::exception& e)
			{
				std::cout << "\033[93m[!] \033[97m"; // Yellow
				std::cout << "Invalid Entry, please enter a number or 'x' to exit. " << std::endl;
			}
		} while (true);
	}
}; 

// Thread pool used for querying the database
class ThreadPool
{
private:
	int THREAD_COUNT = 5;
	bool quit = false;
	
	std::queue <std::function<void()>> tasks;
	std::vector <std::thread> threads;
	std::unordered_map<std::thread::id, bool> working_state;
	
	std::mutex task_mutex;
	std::mutex state_mutex;

	std::condition_variable cv;
	
	void get_task()
	{
		while (!quit)
		{
			
			std::function <void()> task; // Declare task

			{
				std::unique_lock <std::mutex> lock(task_mutex); // Lock mutex
				cv.wait(lock, [this] {return !tasks.empty() || quit; }); // Sleep until new task

				if (quit && tasks.empty())
					return;

				// Get and remove task from queue
				task = std::move(tasks.front());
				tasks.pop(); 
			}
			task(); // Execute the task	
		}
	}

public:
	
	// Constructor
	ThreadPool()
	{
		for (int i = 0; i < THREAD_COUNT; i++)
		{
			threads.emplace_back(std::thread(&ThreadPool::get_task, this));
		}
	}

	// Deconstructor, closes threads
	~ThreadPool()
	{
		{
			std::lock_guard <std::mutex> lock(task_mutex);
			quit = true;	
		}
		
		cv.notify_all();
		for (auto& thread : threads)
		{
			thread.join();
		}	
	}

	// Checks whether threads are still working
	const bool busy()
	{
		std::lock_guard <std::mutex> lock1(task_mutex); 
		
		if (!tasks.empty())
			return true; // Still task(s) to be complete
		
		return false;
	}

	// Adds task to queue
	template <typename Func, typename Arg1, typename Arg2>
	void pushTask(Func&& func, Arg1&& arg1, Arg2&& arg2)
	{
		{
			std::unique_lock <std::mutex> lock(task_mutex);

			// Add task, forward parameters
			tasks.emplace
			(
				std::bind
				(
					std::forward<Func>(func),
					std::forward<Arg1>(arg1),
					std::forward<Arg2>(arg2)
				)
			);
		} 
		cv.notify_one();
	}

}; // End of ThreadPool
#endif // End of file