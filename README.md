# LFG Dungeon Queue Simulator

A multi-threaded C++ simulation of an MMO-style dungeon queue. Users can add players dynamically while dungeons run concurrently.

## Features
- Multiple dungeon instances run simultaneously.
- Automatically forms parties when enough players are in the queue:
  - 1 Tank, 1 Healer, 3 DPS per party.
- Users can add players in real-time (`add <role> <amount>`).
- Thread-safe logging of dungeon activity and queue status.
- Shows current queue before each user input.
- Simulation ends gracefully on `quit` or `exit`.

## How It Works
1. The simulator starts by asking for:
   - Maximum concurrent dungeon instances.
   - Initial number of Tanks, Healers, and DPS in the queue.
   - Minimum and maximum dungeon run times (in seconds).
2. Parties are automatically formed whenever there are enough players and free dungeon instances.
3. Dungeon runs are handled by detached threads that simulate time spent in the dungeon.
4. After the initial queue is processed, users enter the **Manual Control Phase**, where they can add players or quit.
5. All activity is logged with timestamps, including party formation, dungeon completion, and remaining queue.

## Compilation & Running
`g++ main.cpp -o main -std=c++17 -pthread`
`./main # Linux/macOS`
`main.exe # Windows`


## Commands (Manual Control Phase)
`add <role> <amount> # Add players to the queue`
`quit # Exit the simulation`

### Example Usage
`add tank 2`
`add dps 5`
`quit`