#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <queue>
#include <mutex>
#include <csignal>
#include <condition_variable>
std::mutex sockMt;

constexpr int BUFFER_SIZE = 1024;

  // Replace with your server URL
const int deviceCount = 2000;

class ThreadPool {
public:
  ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;

          {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty())
                return;

            task = std::move(tasks.front());
            tasks.pop();
          }

          task(); // Execute the task
        }
      });
    }
  }

  ~ThreadPool() {
    {
     std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
    }

    condition.notify_all();

    for (std::thread& worker : workers)
      worker.join();
}

template <class Func, class... Args>
void enqueue(Func&& func, Args&&... args) {
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      tasks.emplace([func, args...] { func(args...); });
    }
    condition.notify_one();
  }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// Example usage:
void taskFunction(int taskId) {
    std::cout << "Task " << taskId << " is being executed by thread " << std::this_thread::get_id() << std::endl;
}


void sendHexMessage(const std::string& serverIP, int serverPort, const std::vector<std::vector<uint8_t>>& hexBuffers) {
    // Lock the mutex to protect shared resources

    int clientSocket;
    sockaddr_in serverAddress{};
		sockMt.lock();
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		sockMt.unlock();
    if (clientSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddress.sin_port = htons(serverPort);

    int parameter = 0;

    for(int k = 0; k <10; k++){
      parameter = connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress));
      if (parameter >= 0)
        break;
      else{
        std::cerr << "try "<<k<< " Failed to connect to the server" << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    if(parameter < 0 )
      return;
    // if (connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
    //     std::cerr << "Failed to connect to the server" << std::endl;
    //     close(clientSocket);
    //     return;
    // }

    // Send the hexadecimal message to the server
    int i = 0;
    for(; i < hexBuffers.size(); ++i){
      //std::lock_guard<std::mutex> lock(mutex);
      send(clientSocket, hexBuffers[i].data(), hexBuffers[i].size(), 0);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    //Receive the response from the server
    char buffer[BUFFER_SIZE];
    // while(true){
    //   int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    //   if (bytesRead > 0) {
    //       buffer[bytesRead] = '\0';
    //       std::cout << "Received response: " << buffer << std::endl;
    //   }
    // }
    close(clientSocket);
}

std::vector<uint8_t> ascToHex(std::string hex)
{
  // initialize the ASCII code string as empty.
  std::vector<uint8_t> values;
  size_t j = 0;
  for (size_t i = 0; i < hex.length(); i += 2)
  {
    j += 1;
    // extract two characters from hex string
    std::string part = hex.substr(i, 2);

    // change it into base 16 and
    // typecast as the character
    uint8_t ch = std::stoi(part, nullptr, 16);

    // add this char to final array
    values.push_back(ch);
  }
  return values;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    int serverPort = 3670; // Replace with the desired server port
    std::string serverIP = "192.168.18.6"; // Replace with the server IP address



    // Start the TCP server in a separate thread
  std::set<std::string> devices;
  std::vector<std::thread> threads;
  std::ifstream File("/home/mak/repos/multhread/xaa");
  //std::ofstream File2("./suntech_organized.log");
  std::ofstream File3("./suntech_devices2.log");
  std::string message;
  std::vector<std::string> values;
  std::map<std::string,std::vector<std::vector<uint8_t>>> mapDevtoMessage;

  while(std::getline(File, message)){
    size_t pos = message.find(' ');
    if(pos == std::string::npos){
      values.push_back(message);
      continue;
    }
    std::string device = message.substr(0, pos);
    message = message.substr(pos+1);
    std::map<std::string,std::vector<std::vector<uint8_t>>>::iterator it = mapDevtoMessage.find(device);
    devices.insert(device);
    if(it != mapDevtoMessage.end()){
      std::vector<uint8_t> arr = ascToHex(message);
      it->second.push_back(arr);
    }else{
      File3<<device<<"\n";
      std::vector<std::vector<uint8_t>> vect;
      std::vector<uint8_t> arr = ascToHex(message);
      vect.push_back(arr);
      mapDevtoMessage.insert(std::pair<std::string,std::vector<std::vector<uint8_t>> >(device, vect));
    }
  }
  std::map<std::string,std::vector<std::vector<uint8_t>> >::iterator it = mapDevtoMessage.begin();
  int i = 0;
  // for(; it != mapDevtoMessage.end(); it++){
  //   File2 << it->first <<"\n";
  //   std::vector<std::vector<uint8_t>>::iterator itr = it->second.begin();
  //   for(; itr != it->second.end(); itr++){
  //     for (int k = 0; k < itr->size(); k++) {
  //       File2 << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(itr->at(k));
  //     }
  //     File2 << "\n";
  //   }
  //   File2 << "\n\n";
  // }
  //it = mapDevtoMessage.begin();
  const std::string& ip = serverIP;
  int port = serverPort;

  ThreadPool pool(deviceCount);
  while(it != mapDevtoMessage.end()){
      std::vector<std::vector<uint8_t>> message = std::move(it->second);
      //std::function<void()> sender = std::bind(sendHexMessage, ip, port, message);
      pool.enqueue(sendHexMessage,ip,port, message);
      it++;
    }

  File.close();
  //File2.close();
  File3.close();

  return 0;
}
