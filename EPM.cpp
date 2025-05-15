// EPM.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <Windows.h>

using namespace std;

struct HandleDeleter {
	void operator()(HANDLE h) const {
		if (h && h != INVALID_HANDLE_VALUE) {
			CloseHandle(h);
		}
	}
};

std::string GetLastErrorMessage(DWORD errorCode = GetLastError()) {
	LPSTR messageBuffer = nullptr;

	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);
	LocalFree(messageBuffer);

	return message;
}

class ProcessMessage {
private:
	bool read;
	string pipeName;
	HANDLE pipe;

	void createPipe(DWORD mode) {
		pipe = CreateFileA(
			pipeName.c_str(), mode, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (pipe == INVALID_HANDLE_VALUE) {
			std::cerr << "Failed to connect to pipe.Error: " << GetLastError() << '\n';
			std::cerr << GetLastErrorMessage() << endl;
			return;
		}
	}
public:
	ProcessMessage(bool read, string name) : read(read), pipeName(name)
	{
		if (read == true) {
			createPipe(GENERIC_READ);
		}
		else {
			pipe = CreateNamedPipeA(
				pipeName.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
				1, 0, 0, 0, NULL);
		}
	}
	~ProcessMessage() {
		CloseHandle(pipe);
	}

	void writeBuffer(string str) {
		DWORD bytesWritten;
		WriteFile(pipe, str.c_str(), static_cast<DWORD>(str.size()), &bytesWritten, NULL);
	}

	bool readBuffer(string& str) {
		char buffer[1024] = {}; //HANDLE LONGER SIZES LATER
		DWORD bytesRead;
		if (!ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
			return false;
		buffer[bytesRead] = '\0';
		for (int i = 0; i < bytesRead; i++) { //DO something more fancy later
			str.push_back(buffer[i]);
		}
		return true;
	}
	void waitToConnect() {
		ConnectNamedPipe(pipe, NULL);
	}

};

class Classify {
public:


	static int classify(uint16_t num) {
		if (isOdd(num))
			return 1;
		if (prefixOne(num))
			return 2;
		if (hasThreeDigits(num))
			return 3;
		return 4;
	}

	static bool hasThreeDigits(uint16_t num) {
		if (num >= 100) 
			return true;
		return false;
	}

	static bool isOdd(uint16_t num) {
		if (num % 2 == 1)
			return true;
		return false;
	}

	static  void actionOdd(uint16_t num, string& str) {
		str = to_string(num) + ": is an odd number\n";
	}
	static  void action3Digits(uint16_t num, string& str) {
		str = "The product of the number " + to_string(num);
		size_t prod = 1;
		size_t divs = 100;
		while (divs > 0) {
			size_t t = num / divs;
			prod *= t;
			num = num % divs;
			divs = divs / 10;
		}
		str+= " is " + to_string(prod) + "\n";
	}
	// might be faster to convert to a string and take prfix or divide by 10 and 100 and check the quotient
	static bool prefixOne(uint16_t num) {
		if (num == 1)
			return true;
		if (num >= 10 && num < 20)
			return true;
		if (num >= 100 && num < 200)
			return true;
		return false;
	}
	static void actionPrefixOne(uint16_t num, string& str) {
		vector<uint16_t> v;
		str += to_string(num) + " has 1 as a prefix\n";
		generateRandom(num, num, v);
		//cout << "List: ";
		for (auto el : v) {
			str+= to_string(el) + " ";
		}
		str += "\n";
		sort(v.begin(), v.end());
		//cout << "Sorted List: ";
		for (auto el : v) {
			str += to_string(el) + " ";
		}
		str += "\n";
	}
	static void generateRandom(uint16_t num, uint16_t seed, vector<uint16_t>& v) {
		srand(seed);
		for (auto i = 0; i < num; i++) {
			v.push_back(rand() % 1000); 
		}
	}
};
void runChild(string pipeName) {
	ProcessMessage readM(true, pipeName);
	while (1) {
		string str;
		if (readM.readBuffer(str)) {
			if (str.compare("exit") == 0)
				break;
			cout << "Child " << str;
		}
	}
}	

int main(int argc, char**argv)
{
	string pipeName(R"(\\.\pipe\MyNamedPipe)");
	size_t n = 20;
	if (argc > 1 && strcmp(argv[1], "child") == 0) {
		runChild(pipeName);
		return 0;
	}
	//create parent size of pipe
	ProcessMessage writeM(false, pipeName);
	// Start child process with "--child" argument
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	string cmd = string(argv[0]) + " child";
	if (!CreateProcessA(
		NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
		0, NULL, NULL, &si, &pi)) {
		std::cerr << "Parent: CreateProcess failed. Error: " << GetLastError() << '\n';
		return -1;
	}
	
	writeM.waitToConnect();


	vector<uint16_t> v;
	Classify::generateRandom(n, 0, v);
	for (auto el : v) {
		string str;
		int rv = Classify::classify(el);
		switch (rv) {
		case 1: Classify::actionOdd(el, str);
			break;
		case 2: Classify::actionPrefixOne(el, str);
			break;
		case 3: Classify::action3Digits(el, str);
			break;
		case 4:
			str = to_string(el) + " this number belongs to the others\n";
			break;
		default:
			cout << el << " Wasn't classified" << endl;
		}
		writeM.writeBuffer(str);
	}
	writeM.writeBuffer("exit");
	WaitForSingleObject(pi.hProcess, INFINITE);
	return 0;

  
}

