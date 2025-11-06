/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

static unsigned int next_pid = 1;

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue)
{

    std::string trace;              //!< string to store single line of trace file
    std::string execution = "";     //!< string to accumulate the execution output
    std::string system_status = ""; //!< string to accumulate the system status output
    int current_time = time;

    // parse each line of the input trace file. 'for' loop to keep track of indices.
    for (size_t i = 0; i < trace_file.size(); i++)
    {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if (activity == "CPU")
        { // As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        }
        else if (activity == "SYSCALL")
        { // As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }
        else if (activity == "END_IO")
        {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }
        else if (activity == "FORK")
        {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            // Add your FORK output here

            // a. and b. copies the information needed fr4om the PCB of parent process to child process
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            // Create the child PCB
            PCB child_process = current;
            child_process.PID = next_pid;
            next_pid += 1;

            child_process.partition_number = -1;
            if (!allocate_memory(&child_process))
            {
                // Handle fork failure (e.g., no memory)
                execution += std::to_string(current_time) + ", 0, FORK failed: No memory for child process\n";
                // Don't modify 'current' or 'wait_queue', just return from ISR
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;
                continue; // Skip to next trace line
            }

            // Update parent PCB in the wait queue and have child PCB as current running process
            PCB parent_process = current;
            wait_queue.push_back(parent_process);
            current = child_process;

            // c. Scheduler call
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            current_time += 1;

            // Log system status
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace_file[i] + "\n";
            system_status += print_PCB(current, wait_queue) + "\n";

            // d. Return from ISR
            execution += std::to_string(current_time) + ", 1, IRET\n";

            ///////////////////////////////////////////////////////////////////////////////////////////

            // The following loop helps you do 2 things:
            //  * Collect the trace of the child (and only the child, skip parent)
            //  * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            // bool exec_flag = false; // <-- REMOVE
            int parent_index = -1;
            int endif_index = -1; // <-- ADD THIS

            for (size_t j = i + 1; j < trace_file.size(); j++)
            {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);

                if (_activity == "IF_CHILD")
                {
                    skip = false; // Start collecting for child
                    continue;
                }
                else if (_activity == "IF_PARENT")
                {
                    skip = true;      // Stop collecting for child
                    parent_index = j; // Parent resumes here
                    continue;         // Don't add IF_PARENT to any trace
                }
                else if (_activity == "ENDIF")
                {
                    skip = false; // Start collecting post-endif block
                    endif_index = j;
                    if (parent_index == -1)
                    { // Case where there's no IF_PARENT
                        parent_index = j;
                    }
                    continue; // Don't add ENDIF to child trace yet
                }

                if (!skip)
                { // If we are in child block OR post-endif block
                    child_trace.push_back(trace_file[j]);
                }
            }

            // Set the parent's starting point in the outer loop
            if (parent_index != -1)
            {
                i = parent_index; // Parent will resume at IF_PARENT
            }
            else if (endif_index != -1)
            {
                i = endif_index; // Or at ENDIF if no IF_PARENT
            }
            // If neither was found, 'i' just increments, and parent runs from the next line
            ///////////////////////////////////////////////////////////////////////////////////////////
            // With the child's trace, run the child (HINT: think recursion)

            // Save parent PCB, most recent one pushed to the wait queue
            PCB parent_pcb = wait_queue.back();

            // Run child recursively
            auto [child_execution, child_status, child_final_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);

            // Update exectuion and system status logs
            execution += child_execution;
            system_status += child_status;
            current_time = child_final_time;

            // Resume parent process
            wait_queue.pop_back();
            current = parent_pcb;

            ///////////////////////////////////////////////////////////////////////////////////////////
        }
        else if (activity == "EXEC")
        {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            // Add your EXEC output here

            // e. Execute the syscall from Assignment 1 (intr_boilerplate)

            // Free current process memory (as exec overwrites the process)
            if (current.partition_number != -1)
            {
                // free_memory() must be implemented to set the partition code to "free" and PCB partition to -1
                free_memory(&current);
            }

            // f. Search file in file list and obtain memory size
            unsigned int new_program_size = get_size(program_name, external_files);
            // The duration_intr from the trace file is used for the time taken to search the file (e.g., 50ms)
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(new_program_size) + " Mb large\n";
            current_time += duration_intr;

            // g. Find an empty partition where the executable fits
            // Update PCB info needed for allocation
            current.program_name = program_name;
            current.size = new_program_size;
            current.partition_number = -1; // Reset partition before allocation

            // allocate_memory() must be implemented to find a partition and update current.partition_number
            if (!allocate_memory(&current))
            {
                execution += std::to_string(current_time) + ", 0, EXEC failed: Memory allocation failed for " + program_name + "\n";
                system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace_file[i] + "\n";
                system_status += print_PCB(current, wait_queue) + "\n";
                // If allocation fails, the process would typically be terminated or put to sleep, but we'll return to stop the simulation for this branch.
                return {execution, system_status, current_time};
            }

            // h. Simulate the execution of the loader
            int loader_time = new_program_size * 15; // 15ms for every Mb of program
            execution += std::to_string(current_time) + ", " + std::to_string(loader_time) + ", loading program into memory\n";
            current_time += loader_time;

            // i. Mark partition as occupied (3ms from sample log)
            int marking_time = 3;
            execution += std::to_string(current_time) + ", " + std::to_string(marking_time) + ", marking partition as occupied\n";
            current_time += marking_time;

            // j. Update PCB (6ms from sample log)
            int update_time = 6;
            execution += std::to_string(current_time) + ", " + std::to_string(update_time) + ", updating PCB\n";
            current_time += update_time;

            // k. Scheduler call
            execution += std::to_string(current_time) + ", 0, scheduler called\n";

            // k. Return from ISR
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Log system status
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace_file[i] + "\n";
            system_status += print_PCB(current, wait_queue) + "\n";

            ///////////////////////////////////////////////////////////////////////////////////////////

            // l. Run the new program
            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while (std::getline(exec_trace_file, exec_trace))
            {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            // With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            auto [sub_execution, sub_system_status, new_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);

            execution += sub_execution;
            system_status += sub_system_status;
            current_time = new_time;

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; // Why is this important? (answer in report)
        }
    }

    // The current PCB is the one that just finished execution, so we free its memory
    if (current.partition_number != -1)
    {
        free_memory(&current);
    }

    return {execution, system_status, current_time};
}

int main(int argc, char **argv)
{

    // vectors is a C++ std::vector of strings that contain the address of the ISR
    // delays  is a C++ std::vector of ints that contain the delays of each device
    // the index of these elemens is the device number, starting from 0
    // external_files is a C++ std::vector of the struct 'external_file'. Check the struct in
    // interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    // Just a sanity check to know what files you have
    print_external_files(external_files);

    // Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    // Update memory (partition is assigned here, you must implement this function)
    if (!allocate_memory(&current))
    {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/

    /******************************************************************/

    // Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while (std::getline(input_file, trace))
    {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file,
                                                        0,
                                                        vectors,
                                                        delays,
                                                        external_files,
                                                        current,
                                                        wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
