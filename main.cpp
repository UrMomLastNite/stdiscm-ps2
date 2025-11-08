#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <string>
#include <atomic>
#include <algorithm> 
#include <sstream>   
#include <iomanip>   

struct DungeonInstance {
    int id;
    std::string status;
    int parties_served;
    long long total_time_served;
    DungeonInstance(int i) : id(i), status("empty"), parties_served(0), total_time_served(0) {}
};

int tank_queue;
int healer_queue;
int dps_queue;
int min_time;
int max_time;

std::vector<DungeonInstance> instances;
std::atomic<int> active_parties(0);
bool all_players_queued = false;

std::mutex g_mutex;
std::condition_variable cv;
std::mutex rng_mutex;
std::mutex cout_mutex; 

std::chrono::steady_clock::time_point start_time; 

void log_message(const std::string& thread_name, const std::string& message) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    double seconds = elapsed.count() / 1000.0;

    std::cout << "[" << std::fixed << std::setw(8) << std::setprecision(3) << seconds << "s] "
              << "[" << std::setw(15) << thread_name << "] "
              << message << std::endl;
}

// --- Forward Declarations ---
void dungeon_run(int instance_id);
void party_former();
void print_status(const std::string& thread_name); 
bool can_form_party();
int find_free_instance();

// --- Function Implementations (with logging changes) ---

int get_random_time() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min_time, max_time);
    std::lock_guard<std::mutex> lock(rng_mutex);
    return distrib(gen);
}

int main() {
    start_time = std::chrono::steady_clock::now(); // <<< CHANGE: Initialize start time
    const std::string thread_name = "MainThread";

    int n;
    log_message(thread_name, "--- LFG Dungeon Queue Simulator ---");
    std::cout << "Enter max number of concurrent instances (n): ";
    std::cin >> n;
    std::cout << "Enter number of tanks in queue (t): ";
    std::cin >> tank_queue;
    std::cout << "Enter number of healers in queue (h): ";
    std::cin >> healer_queue;
    std::cout << "Enter number of DPS in queue (d): ";
    std::cin >> dps_queue;
    std::cout << "Enter minimum dungeon time in seconds (t1): ";
    std::cin >> min_time;
    std::cout << "Enter maximum dungeon time in seconds (t2): ";
    std::cin >> max_time;

    if (min_time > max_time) {
        log_message(thread_name, "Warning: Min time > Max time. Swapping values.");
        std::swap(min_time, max_time);
    }
    
    log_message(thread_name, "----------------------------------------");

    for (int i = 0; i < n; ++i) {
        instances.emplace_back(i);
    }
    
    std::stringstream ss;
    ss << "Initial Queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D";
    log_message(thread_name, ss.str());
    log_message(thread_name, "Initial Instance Status:");
    print_status(thread_name);
    log_message(thread_name, "----------------------------------------");

    std::thread former_thread(party_former);

    {
        std::unique_lock<std::mutex> lock(g_mutex);
        all_players_queued = true;
        cv.notify_all(); 
        cv.wait(lock, [] {
            return active_parties == 0 && !can_form_party();
        });
    }

    if (former_thread.joinable()) {
        former_thread.join();
    }
    
    log_message(thread_name, "----------------------------------------");
    log_message(thread_name, "Simulation finished. No more parties can be formed.");
    log_message(thread_name, "--- Final Instance Summary ---");
    for (const auto& instance : instances) {
        ss.str(""); ss.clear();
        ss << "Instance " << instance.id << ": Served " << instance.parties_served 
           << " parties. Total time active: " << instance.total_time_served << "s.";
        log_message(thread_name, ss.str());
    }
    ss.str(""); ss.clear();
    ss << "Remaining players in queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D";
    log_message(thread_name, ss.str());

    return 0;
}

void party_former() {
    const std::string thread_name = "PartyFormer";
    while (true) {
        std::unique_lock<std::mutex> lock(g_mutex);
        cv.wait(lock, [] {
            bool can_make_party = can_form_party();
            bool is_finished = (active_parties == 0 && !can_make_party && all_players_queued);
            return (can_make_party && find_free_instance() != -1) || is_finished;
        });

        if (active_parties == 0 && !can_form_party()) {
            log_message(thread_name, "No more parties can be formed. Exiting.");
            return;
        }

        int instance_id = find_free_instance();
        if (can_form_party() && instance_id != -1) {
            tank_queue--;
            healer_queue--;
            dps_queue -= 3;
            instances[instance_id].status = "active";
            active_parties++;

            std::stringstream ss;
            ss << "Party formed! Assigning to Instance " << instance_id 
               << ". Remaining Queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D";
            log_message(thread_name, ss.str());
            
            print_status(thread_name);
            log_message(thread_name, "----------------------------------------");

            std::thread(dungeon_run, instance_id).detach();
        }
    }
}

void dungeon_run(int instance_id) {
    std::stringstream thread_name_ss;
    thread_name_ss << "DungeonRun-" << instance_id;
    const std::string thread_name = thread_name_ss.str();

    int time_in_dungeon = get_random_time();
    log_message(thread_name, "Entering dungeon for " + std::to_string(time_in_dungeon) + "s.");
    
    std::this_thread::sleep_for(std::chrono::seconds(time_in_dungeon));
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        instances[instance_id].status = "empty";
        instances[instance_id].parties_served++;
        instances[instance_id].total_time_served += time_in_dungeon;
        active_parties--;

        std::stringstream ss;
        ss << "Instance " << instance_id << " is now free after " << time_in_dungeon << "s. "
           << active_parties << " parties still active.";
        log_message(thread_name, ss.str());

        print_status(thread_name);
        log_message(thread_name, "----------------------------------------");
    }
    
    cv.notify_all();
}

bool can_form_party() {
    return tank_queue >= 1 && healer_queue >= 1 && dps_queue >= 3;
}

int find_free_instance() {
    for (size_t i = 0; i < instances.size(); ++i) {
        if (instances[i].status == "empty") {
            return i;
        }
    }
    return -1;
}

void print_status(const std::string& thread_name) {
    for (const auto& instance : instances) {
        std::stringstream ss;
        ss << "  Instance " << instance.id << ": " << instance.status;
        log_message(thread_name, ss.str());
    }
}