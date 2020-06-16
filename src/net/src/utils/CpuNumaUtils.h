#pragma once

#include <algorithm>
#include <sstream>
#include <fstream>
#include <numa.h>
#include <numaif.h>
#include <vector>
#include <iostream>

namespace rdma
{
class CpuNumaUtils {
public: 
static std::vector<std::vector<int>> get_cpu_numa_map(int &num_cpus, int &num_nodes) {
    int ncpus_ = numa_num_task_cpus();
    int nnodes_ = numa_max_node() + 1;
    int nphycpus_ = 0;
    static bool stats_printed = true;
    if (!stats_printed)
       printf("We are running on %d nodes and %d CPUs\n", nnodes_, ncpus_);
    auto cpuids_ = new std::vector<std::vector<int> >[nnodes_];
    {
        std::vector<int> known_siblings;
        for (unsigned cpu = 0; cpu < (unsigned)ncpus_; ++cpu) {
            // skip a core if it is a hyper-threaded logical core
            if (std::find(known_siblings.begin(), known_siblings.end(), cpu) != known_siblings.end())
                continue;
            // if this core is not known as a sibling, add it to the core vector of its node
            int node = numa_node_of_cpu(cpu);
            std::vector<int> phy_core;
            // find out the siblings of this core
            std::stringstream path;
            path << "/sys/devices/system/cpu/cpu" << cpu << "/topology/thread_siblings_list";
            std::ifstream f(path.str().c_str());
            if (f) {
                std::string siblings((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                f.close();
                // std::cout << "CPU " << cpu << " siblings:" << siblings << std::endl;
                std::istringstream ss(siblings);
                int coreid;
                while(ss >> coreid) {
                    known_siblings.push_back(coreid);
                    phy_core.push_back(coreid);
                    ss.ignore();
                }
            } else {
                phy_core.push_back(cpu);
            }
            cpuids_[node].push_back(phy_core);
        }
    }
    if (!stats_printed)
       printf("CPU Topology: \n");
    for (unsigned i = 0; i < (unsigned)nnodes_; ++i) {
        if (!stats_printed)
              printf("node %d:\t", i);
        for (std::vector<std::vector<int> >::const_iterator cpu = cpuids_[i].begin(); cpu != cpuids_[i].end(); ++cpu) {
            if (!stats_printed) {
                printf("[ ");
                for (std::vector<int>::const_iterator t = (*cpu).begin(); t != (*cpu).end(); ++t)
                    printf("%d ", *t);
                printf("] ");
            }
        }
        if (!stats_printed)
              putchar('\n');
        nphycpus_ += cpuids_[i].size();
    }
    if (!stats_printed)
       printf("%d physical cores total.\n", nphycpus_);
    int node_count = numa_max_node() + 1;
    for (int i = 0; i < node_count; ++i) {
        long fr;
        unsigned long sz = numa_node_size(i, &fr);
        if (!stats_printed)
            printf("Node %d: %13lu Available %13ld Free\n", i, sz, fr);
    }
    stats_printed = true;
    std::vector<std::vector<int>> cpu_map;
    cpu_map.resize(nnodes_);
    for (unsigned i = 0; i < (unsigned)nnodes_; i++) {
        for (std::vector<std::vector<int> >::const_iterator cpu = cpuids_[i].begin(); cpu != cpuids_[i].end(); ++cpu) {
            cpu_map[i].push_back((*cpu)[0]);
        }
    }
    delete[] cpuids_;
    num_cpus = nphycpus_;
    num_nodes = nnodes_;
    return cpu_map;
};

static int get_numa_node_from_ptr(void *ptr) {
    int numa_node = -1;
    if (get_mempolicy(&numa_node, NULL, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR) < 0)
        std::cout << "WARNING: get_mempolicy failed" << std::endl;
    return numa_node;
};
static long numa_node_free_mem(int node) {
    long _free = 0;
    numa_node_size(node, &_free);
    return _free;
};

};
}