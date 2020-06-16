

#include "Config.h"
#include "Logging.h"
#include <cmath>

using namespace rdma;

//TEST
int Config::HELLO_PORT = 4001;


//RDMA
size_t Config::RDMA_MEMSIZE = 1024ul * 1024 * 1024 * 5;  //1GB
uint32_t Config::RDMA_NUMAREGION = 1;
std::string Config::RDMA_DEVICE_FILE_PATH;
uint32_t Config::RDMA_IBPORT = 1;
uint32_t Config::RDMA_PORT = 5200;
uint32_t Config::RDMA_MAX_WR = 4096;
uint32_t Config::RDMA_MAX_SRQ_WR = 32767;

uint32_t Config::RDMA_UD_MTU = 4096;

std::string Config::SEQUENCER_IP = "192.168.94.28"; //node08
uint32_t Config::SEQUENCER_PORT = 5500;

std::string Config::RDMA_INTERFACE = "ib0";

//SYSTEM
uint32_t Config::CACHELINE_SIZE = 64;

//THREADING
vector<vector<int>> Config::NUMA_THREAD_CPUS = {{0,1,2,3,4,5,6,7,8,9,10,11,12,13}, {14,15,16,17,18,19,20,21,22,23,24,25,26,27}}; //DM-cluster cpus

//LOGGING
int Config::LOGGING_LEVEL = 3;

// string& Config::getIPFromNodeId(NodeID& node_id){
//   return Config::DPI_NODES.at(node_id -1);
// }
// string& Config::getIPFromNodeId(const NodeID& node_id){
//   return Config::DPI_NODES.at(node_id -1);
// }


inline string trim(string str) {
  str.erase(0, str.find_first_not_of(' '));
  str.erase(str.find_last_not_of(' ') + 1);
  return str;
}

void Config::init_vector(vector<string>& values, string csv_list) {
  values.clear();
  char* csv_clist = new char[csv_list.length() + 1];
  strcpy(csv_clist, csv_list.c_str());
  char* token = strtok(csv_clist, ",");

  while (token) {
    values.push_back(token);
    token = strtok(nullptr, ",");
  }

  delete[] csv_clist;
}

void Config::init_vector(vector<int>& values, string csv_list) {
  values.clear();
  char* csv_clist = new char[csv_list.length() + 1];
  strcpy(csv_clist, csv_list.c_str());
  char* token = strtok(csv_clist, ",");

  while (token) {
    string value(token);
    values.push_back(stoi(value));
    token = strtok(nullptr, ",");
  }

  delete[] csv_clist;
}

void Config::unload() {
  google::protobuf::ShutdownProtobufLibrary();
}

void Config::load(const string& prog_name) {
  string conf_file;
  if (prog_name.empty() || prog_name.find("/") == string::npos) {
    conf_file = ".";
  } else {
    conf_file = prog_name.substr(0, prog_name.find_last_of("/"));
  }
  conf_file += "/conf/RDMA.conf";

  ifstream file(conf_file.c_str());

  if (file.fail()) {
    Logging::error(__FILE__, __LINE__,
                    "Failed to load config file at " + conf_file + ". "
                    "The default values are used.");
  }

  string line;
  string key;
  string value;
  int posEqual;
  while (getline(file, line)) {

    if (line.length() == 0)
      continue;

    if (line[0] == '#')
      continue;
    if (line[0] == ';')
      continue;

    posEqual = line.find('=');
    key = line.substr(0, posEqual);
    value = line.substr(posEqual + 1);
    set(trim(key), trim(value));
  }
}

void Config::set(string key, string value) {
  //config
  if (key.compare("RDMA_PORT") == 0) {
    Config::RDMA_PORT = stoi(value);
  } else if (key.compare("RDMA_MEMSIZE") == 0) {
    Config::RDMA_MEMSIZE = strtoul(value.c_str(), nullptr, 0);
  } else if (key.compare("RDMA_NUMAREGION") == 0) {
    Config::RDMA_NUMAREGION = stoi(value);
  } else if (key.compare("RDMA_IBPORT") == 0) {
    Config::RDMA_IBPORT = stoi(value);
  }else if (key.compare("LOGGING_LEVEL") == 0) {
    Config::LOGGING_LEVEL = stoi(value);
  }else if (key.compare("CACHELINE_SIZE") == 0) {
    Config::CACHELINE_SIZE = stoi(value);
  }
}


string Config::getIP(std::string &interface) {
  int fd;
  struct ifreq ifr;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  /* I want to get an IPv4 IP address */
  ifr.ifr_addr.sa_family = AF_INET;
  /* I want an IP address attached to interface */
  strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ-1);

  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);

  return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}
