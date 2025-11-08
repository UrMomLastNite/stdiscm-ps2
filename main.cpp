#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <string>
#include <atomic>

// Structure to hold information about a single dungeon instance
struct DungeonInstance {
    int id;
    std::string status;
    int parties_served;
    long long total_time_served;

    DungeonInstance(int i) : id(i), status("empty"), parties_served(0), total_time_served(0) {}
};

// --- Shared State ---
// These variables are accessed by multiple threads and must be protected.

// Input variables
int tank_queue;
int healer_queue;
int dps_queue;
int min_time;
int max_time;

// System state
std::vector<DungeonInstance> instances;
std::atomic<int> active_parties(0); // Count of parties currently in a dungeon
bool all_players_queued = false;  // Flag to signal that initial players are set

// --- Synchronization Primitives ---
std::mutex g_mutex; // A single global mutex to protect all shared data (queues and instances)
std::condition_variable cv; // A condition variable to signal changes in state

// --- Forward Declarations ---
void dungeon_run(int instance_id);
void party_former();
void print_status();
bool can_form_party();
int find_free_instance();

// A simple random number generator for dungeon clear times
int get_random_time() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min_time, max_time);
    return distrib(gen);
}

// Main function
int main() {
    // --- Input ---
    int n; // max instances
    std::cout << "--- LFG Dungeon Queue Simulator ---\n";
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
    std::cout << "\n----------------------------------------\n";

    // --- Initialization ---
    for (int i = 0; i < n; ++i) {
        instances.emplace_back(i);
    }
    
    std::cout << "Initial Queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D\n";
    std::cout << "Initial Instance Status:\n";
    print_status();
    std::cout << "----------------------------------------\n";

    // --- Start Simulation ---
    // Start the thread that forms parties
    std::thread former_thread(party_former);

    // This lock is for the main thread to wait on the condition variable
    std::unique_lock<std::mutex> lock(g_mutex);
    all_players_queued = true;
    
    // Notify the party former that initial players are ready
    cv.notify_all(); 

    // Main thread waits until all activity has ceased.
    // The condition for completion is:
    // 1. No parties are currently active in dungeons.
    // 2. No more parties can possibly be formed from the remaining players in the queue.
    cv.wait(lock, [] {
        return active_parties == 0 && !can_form_party();
    });

    // --- Shutdown ---
    // Wait for the party_former thread to finish its work and exit cleanly.
    if (former_thread.joinable()) {
        former_thread.join();
    }
    
    std::cout << "\n----------------------------------------\n";
    std::cout << "Simulation finished. No more parties can be formed.\n";

    // --- Final Summary ---
    std::cout << "\n--- Final Instance Summary ---\n";
    for (const auto& instance : instances) {
        std::cout << "Instance " << instance.id << ": Served " << instance.parties_served 
                  << " parties. Total time active: " << instance.total_time_served << "s.\n";
    }
    std::cout << "Remaining players in queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D\n";

    return 0;
}

// Thread function for the main party-forming logic
void party_former() {
    while (true) {
        std::unique_lock<std::mutex> lock(g_mutex);

        // Wait until one of two conditions is met:
        // 1. A party can be formed AND an instance is available.
        // 2. The simulation is over (no more parties can ever be formed).
        cv.wait(lock, [] {
            bool can_make_party = can_form_party();
            bool is_finished = (active_parties == 0 && !can_make_party && all_players_queued);
            return (can_make_party && find_free_instance() != -1) || is_finished;
        });

        // Check if the simulation is over and we should exit
        if (active_parties == 0 && !can_form_party()) {
            return; // Exit the thread
        }

        // It's possible we woke up but another thread acted first. Double-check.
        int instance_id = find_free_instance();
        if (can_form_party() && instance_id != -1) {
            // A party can be formed and an instance is available. Let's do it.
            
            // 1. Consume players from the queue
            tank_queue--;
            healer_queue--;
            dps_queue -= 3;
            
            // 2. Assign the party to the instance
            instances[instance_id].status = "active";
            active_parties++;

            std::cout << "\nParty formed! Assigning to Instance " << instance_id 
                      << ". Remaining Queue: " << tank_queue << "T, " << healer_queue << "H, " << dps_queue << "D\n";
            print_status();
            std::cout << "----------------------------------------\n";

            // 3. Spawn a new thread to simulate the dungeon run.
            //    We detach the thread because we don't need to wait for it here.
            //    It will run concurrently and manage its own lifecycle.
            std::thread(dungeon_run, instance_id).detach();
        }
        // If conditions are not met, the loop will continue and wait again.
    }
}

// Thread function that simulates a single party's run through a dungeon
void dungeon_run(int instance_id) {
    int time_in_dungeon = get_random_time();
    
    // Simulate the time spent in the dungeon
    std::this_thread::sleep_for(std::chrono::seconds(time_in_dungeon));
    
    // The run is over. Acquire the lock to update the shared state.
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        
        // Update the instance's stats
        instances[instance_id].status = "empty";
        instances[instance_id].parties_served++;
        instances[instance_id].total_time_served += time_in_dungeon;
        active_parties--;

        std::cout << "\nInstance " << instance_id << " is now free after " << time_in_dungeon << "s. "
                  << active_parties << " parties still active.\n";
        print_status();
        std::cout << "----------------------------------------\n";
    } // Lock is released here
    
    // Notify all waiting threads (the party_former and the main thread)
    // that the state has changed. An instance is free, and the active party count decreased.
    cv.notify_all();
}

// Helper function to check if a party (1T, 1H, 3D) can be formed
bool can_form_party() {
    return tank_queue >= 1 && healer_queue >= 1 && dps_queue >= 3;
}

// Helper function to find the ID of a free instance. Returns -1 if none are free.
int find_free_instance() {
    for (size_t i = 0; i < instances.size(); ++i) {
        if (instances[i].status == "empty") {
            return i;
        }
    }
    return -1;
}

// Helper function to print the current status of all instances.
// Assumes the caller holds the lock `g_mutex`.
void print_status() {
    for (const auto& instance : instances) {
        std::cout << "Instance " << instance.id << ": " << instance.status << "\n";
    }
}