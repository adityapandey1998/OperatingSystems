#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <set>
#include <stdio.h>
#include <unistd.h>

using namespace std;
int quantum = 10000, maxprio;
bool debug_flag=false;;
int myrandom(int burst);
void read_input_file(char *filename);
void print_summary();
vector<int> read_random_file(string filename);
void print_event_queue();
typedef enum
{
    STATE_RUNNING,
    STATE_BLOCKED,
    STATE_CREATED,
    STATE_READY,
    STATE_DONE,
    STATE_PREEMPT,
} process_state_t;

string STATE_NAMES[] = {"RUNNING", "BLOCK", "CREATED", "READY", "DONE", "PREEMPT"};
typedef enum
{
    TRANS_TO_READY,
    TRANS_TO_RUN,
    TRANS_TO_BLOCK,
    TRANS_TO_PREEMPT,
    TRANS_TO_DONE
} event_transition_t;

vector<int> rand_vals;
int rsize;
class Event;
class Process
{
public:
    int id;
    int arrival_time, total_cpu_time, cpu_burst_lim, io_burst_lim;
    int cpu_burst, io_burst;
    int rem_time, rem_cpu_burst;
    int finish_time;
    int io_time;
    int wait_time;
    int cur_state_time;
    int prev_state_duration;
    int ready_queue_time;
    process_state_t cur_state;
    int static_priority, dynamic_priority;
    bool preempted, priority_reset;

    Process(int arrival_time, int total_cpu_time, int cpu_burst_lim, int io_burst_lim, int maxprio)
    {
        static int global_id = 0;
        this->id = global_id++;
        this->arrival_time = arrival_time;
        this->total_cpu_time = total_cpu_time;
        this->rem_time = total_cpu_time;
        this->cpu_burst_lim = cpu_burst_lim;
        this->io_burst_lim = io_burst_lim;
        this->io_burst = 0;
        this->io_time = 0;
        this->wait_time = 0;
        this->ready_queue_time = 0;
        this->static_priority = myrandom(maxprio);
        this->dynamic_priority = this->static_priority - 1;
        this->cur_state_time = arrival_time;
        this->cur_state = STATE_READY;
        this->preempted = false;
    }

    void update_priority()
    {
        this->dynamic_priority--;
    }
    void reset_priority()
    {
        this->dynamic_priority = this->static_priority - 1;
    }
};
vector<Process> process_list;

class Event
{
public:
    Process *evtProcess;
    int evtTimeStamp;
    int prevTimeStamp;
    int eventNum;
    event_transition_t transition;
    process_state_t curState;
    process_state_t prevState;

    Event(Process *evtProcess, int evtTimeStamp, int prevTimeStamp, process_state_t curState, process_state_t prevState, int eventNum)
    {
        this->evtProcess = evtProcess;
        this->evtTimeStamp = evtTimeStamp;
        this->prevTimeStamp = prevTimeStamp;
        this->curState = curState;
        this->prevState = prevState;
        this->eventNum = eventNum;
    }
};

int newEventNumber()
{
    static int eventCount = 0;
    eventCount++;
    return eventCount;
}
struct EventComparator
{
    bool operator()(const Event *event1, const Event *event2)
    {
        if (event1->evtTimeStamp != event2->evtTimeStamp)
        {
            return event1->evtTimeStamp > event2->evtTimeStamp;
        }
        return event1->eventNum > event2->eventNum;
    }
};
priority_queue<Event *, vector<Event *>, EventComparator> eventQueue;

Event *get_event()
{
    if (eventQueue.empty())
        return nullptr;
    Event *temp = eventQueue.top();
    eventQueue.pop();
    return temp;
}
void add_event(Event *evt)
{
    eventQueue.push(evt);
    if (debug_flag==true)
        print_event_queue();
}
void rm_event()
{
    eventQueue.pop();
}
int get_next_event_time()
{
    if (eventQueue.empty())
        return -1;
    Event *evt = eventQueue.top();
    int next_event_time = evt->evtTimeStamp;
    evt = nullptr;
    return next_event_time;
}

void print_event_queue()
{
    priority_queue<Event *, vector<Event *>, EventComparator> eventQueue2 = eventQueue;
    while (!eventQueue2.empty())
    {
        cout << "===" << eventQueue2.top()->evtProcess->id << ":" << eventQueue2.top()->evtTimeStamp << " - Dyn Priority: " <<eventQueue2.top()->evtProcess->dynamic_priority << " || "
             << "State: " << STATE_NAMES[eventQueue2.top()->curState] << endl;
        eventQueue2.pop();
    }
    cout << endl;
}

class Scheduler
{
public:
    virtual void add_process(Process *proc) = 0;
    virtual Process *get_next_process() = 0;
    // virtual bool test_preempt(Process *p, int current_time) = 0;
};
Scheduler *THE_SCHEDULER;

class FCFS : public Scheduler
{
    deque<Process *> runQueue;

public:
    void add_process(Process *proc)
    {
        runQueue.push_back(proc);
        proc->dynamic_priority = proc->static_priority - 1;
    }
    Process *get_next_process()
    {
        if (runQueue.empty())
            return nullptr;
        Process *temp = runQueue.front();
        runQueue.pop_front();
        return temp;
    }
};

class SRTF : public Scheduler
{
    deque<Process *> runQueue;
    Process *temp_proc;

public:
    void add_process(Process *proc)
    {
        int i;
        for (i = 0; i < runQueue.size(); i++)
        {
            temp_proc = runQueue[i];
            if (temp_proc->rem_time > proc->rem_time)
                break;
        }
        runQueue.insert(runQueue.begin() + i, proc);
        proc->dynamic_priority = proc->static_priority - 1;
    }
    Process *get_next_process()
    {
        if (runQueue.empty())
            return nullptr;
        Process *temp = runQueue.front();
        runQueue.pop_front();
        return temp;
    }
};

class RR : public FCFS
{
};

class LCFS : public Scheduler
{
    deque<Process *> runQueue;

public:
    void add_process(Process *proc)
    {
        runQueue.push_back(proc);
        proc->dynamic_priority = proc->static_priority - 1;
    }
    Process *get_next_process()
    {
        if (runQueue.empty())
            return nullptr;
        Process *temp = runQueue.back();
        runQueue.pop_back();
        return temp;
    }
};

class PRIO : public Scheduler
{
    deque<Process *> *activeQueue, *expiredQueue;
    int max_priority;
public:
    PRIO(int max_priority)
    {
        this->max_priority = max_priority;
        this->activeQueue = new deque<Process *>[max_priority];
        this->expiredQueue = new deque<Process *>[max_priority];
    }

    void add_process(Process *proc)
    {
        if (proc->dynamic_priority<0)
        {
            proc->dynamic_priority = proc->static_priority - 1;
            this->expiredQueue[proc->dynamic_priority].push_back(proc);
        }
        else
            this->activeQueue[proc->dynamic_priority].push_back(proc);
    }

    Process *get_next_process()
    {
        deque<Process *> *currentQueue, *tempQueue;

        for (int i = max_priority - 1; i >= 0; i--)
        {
            tempQueue = &(this->activeQueue[i]);
            if (!tempQueue->empty())
            {
                Process *temp_proc = tempQueue->front();
                tempQueue->pop_front();
                return temp_proc;
            }
        }
        if (debug_flag==true)
            cout << "Switching Active and Empty Queue" << endl;
        currentQueue = this->activeQueue;
        this->activeQueue = this->expiredQueue;
        this->expiredQueue = currentQueue;

        for (int i = max_priority - 1; i >= 0; i--)
        {
            tempQueue = &(this->activeQueue[i]);
            if (!tempQueue->empty())
            {
                Process *temp_proc = tempQueue->front();
                tempQueue->pop_front();
                return temp_proc;
            }
        }
        return nullptr;
    }
};

int io_count = 0, total_io_time = 0;
void Simulation()
{
    if (debug_flag==true)
        cout << "In Simulation" << endl;
    Event *evt;
    Process *CURRENT_RUNNING_PROCESS = nullptr;
    bool CALL_SCHEDULER = false;
    int CURRENT_TIME, prev_state_dur, io_temp = 0;
    while ((evt = get_event()) != nullptr)
    {
        Process *proc = evt->evtProcess;
        CURRENT_TIME = evt->evtTimeStamp;
        prev_state_dur = CURRENT_TIME - evt->prevTimeStamp;
        switch (evt->curState)
        {
        case STATE_READY:
        {
            if (debug_flag==true)
                cout << "---In STATE_READY" << endl;

            if (evt->prevState == STATE_BLOCKED)
            {
                io_count--;
                if (io_count == 0)
                    total_io_time += CURRENT_TIME - io_temp;
                proc->io_time += prev_state_dur;
            }
            proc->dynamic_priority = proc->static_priority - 1;
            CALL_SCHEDULER = true;
            proc->ready_queue_time = CURRENT_TIME;
            THE_SCHEDULER->add_process(proc);
            break;
        }

        case STATE_RUNNING:
        {
            if (debug_flag==true)
                cout << "---In STATE_RUNNING" << endl;
            proc->wait_time += prev_state_dur;
            int new_cpu_burst;
            if (proc->preempted && proc->rem_cpu_burst != 0)
                new_cpu_burst = proc->rem_cpu_burst;
            else {
                new_cpu_burst = myrandom(proc->cpu_burst_lim);

                if (new_cpu_burst >= proc->rem_time)
                    new_cpu_burst = proc->rem_time;
                proc->rem_cpu_burst = new_cpu_burst;
            }

            Event *newEvent;
            int newEventTime = 0;
            process_state_t newState;
            if (new_cpu_burst > quantum)
            {
                newEventTime = CURRENT_TIME + quantum;
                newState = STATE_PREEMPT;
            }
            else
            {
                newEventTime = CURRENT_TIME + new_cpu_burst;
                if (new_cpu_burst == proc->rem_time)
                    newState = STATE_DONE;
                else
                    newState = STATE_BLOCKED;
            }
            Event *newEvt;
            if (new_cpu_burst > quantum)
            {
                newEvt = new Event(proc, CURRENT_TIME + quantum, CURRENT_TIME, STATE_PREEMPT, STATE_RUNNING, newEventNumber());
            }
            else if (new_cpu_burst == proc->rem_time)
            {
                newEvt = new Event(proc, CURRENT_TIME + new_cpu_burst, CURRENT_TIME, STATE_DONE, STATE_RUNNING, newEventNumber());
            }
            else
            {
                newEvt = new Event(proc, CURRENT_TIME + new_cpu_burst, CURRENT_TIME, STATE_BLOCKED, STATE_RUNNING, newEventNumber());
            }

            add_event(newEvt);
            break;
        }

        case STATE_BLOCKED:
        {
            if (debug_flag==true)
                cout << "---In STATE_BLOCKED" << endl;

            Event *newEvent;
            proc->preempted = false;

            CURRENT_RUNNING_PROCESS = nullptr;
            if (io_count == 0)
                io_temp = CURRENT_TIME;

            io_count++;
            int io_burst = myrandom(proc->io_burst_lim);
            proc->rem_time -= prev_state_dur;
            int event_fire_time = CURRENT_TIME + io_burst;
            newEvent = new Event(proc, event_fire_time, CURRENT_TIME, STATE_READY, STATE_BLOCKED, newEventNumber());
            add_event(newEvent);
            CALL_SCHEDULER = true;
            break;
        }

        case STATE_CREATED:
            break;

        case STATE_DONE:
        {
            if (debug_flag==true)
                cout << "---In STATE_DONE" << endl;
            CURRENT_RUNNING_PROCESS = nullptr;
            CALL_SCHEDULER = true;
            proc->finish_time = CURRENT_TIME;
            break;
        }

        case STATE_PREEMPT:
        { 
            if (debug_flag==true)
                cout << "---In STATE_PREEMPT" << endl;

            CURRENT_RUNNING_PROCESS = nullptr;
            CALL_SCHEDULER = true;
            proc->preempted = true;
            proc->ready_queue_time = CURRENT_TIME;
            proc->rem_time -= prev_state_dur;
            proc->rem_cpu_burst -= prev_state_dur;
            // --
            proc->update_priority();
            THE_SCHEDULER->add_process(proc);
            if (debug_flag==true)
                cout << "---End STATE_PREEMPT" << endl;
            break;
        }
        }

        delete evt;
        evt = nullptr;
        if (CALL_SCHEDULER == true)
        {
            if (get_next_event_time() == CURRENT_TIME)
                continue;
            
            CALL_SCHEDULER = false;
            if (CURRENT_RUNNING_PROCESS == nullptr)
            {
                CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
                if (CURRENT_RUNNING_PROCESS == nullptr)
                    continue;
                Event *newEvent = new Event(CURRENT_RUNNING_PROCESS, CURRENT_TIME, CURRENT_RUNNING_PROCESS->ready_queue_time, STATE_RUNNING, STATE_READY, newEventNumber());
                add_event(newEvent);
            }
        }
    }
}

int main(int argc, char **argv)
{
    char *input_file = argv[argc - 2];
    char *rand_file = argv[argc - 1];
    if (debug_flag==true)
        cout << input_file << " " << rand_file << endl;
    int c;
    char *cvalue = NULL;
    while ((c = getopt(argc, argv, "vs:")) != -1)
        switch (c)
        {
        case 's':
            cvalue = optarg;
            break;
        case 'v':
            debug_flag=true;
            break;
        case '?':
            if (optopt == 's')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            return 1;
        default:
            abort();
        }
    if (debug_flag==true)
        cout << "Value with -s: " << optarg << endl;
    char sched_type = optarg[0];
    char ch;
    rand_vals = read_random_file(rand_file);
    maxprio=4;

    switch (sched_type)
    {
        case 'F':
            cout << "FCFS" << endl;
            read_input_file(input_file);
            THE_SCHEDULER = new FCFS();
            break;
        
        case 'L':
            cout << "LCFS" << endl;
            read_input_file(input_file);
            THE_SCHEDULER = new LCFS();
            break;
        
        case 'S':
            cout << "SRTF" << endl;
            read_input_file(input_file);
            THE_SCHEDULER = new SRTF();
            break;
        
        case 'R':
            sscanf(optarg, "%c%d", &ch, &quantum);
            cout << "RR " << quantum << endl;
            read_input_file(input_file);
            THE_SCHEDULER = new RR();
            break;
        
        case 'P':
            sscanf(optarg, "%c%d:%d", &ch, &quantum, &maxprio);
            cout << "PRIO " << quantum << endl;
            read_input_file(input_file);
            THE_SCHEDULER = new PRIO(maxprio);
            break;
        
        case 'E':
            sscanf(optarg, "%c%d:%d", &ch, &quantum, &maxprio);
            cout << "PREPRIO " << quantum << endl;
            read_input_file(input_file);
            break;
    }

    Simulation();
    print_summary();
}

vector<int> read_random_file(string filename)
{
    ifstream rand_file(filename);
    vector<int> rand_vals;
    int size, n;
    rand_file >> size;
    rsize = size;
    while (rand_file >> n)
    {
        rand_vals.push_back(n);
    }
    return rand_vals;
}

int myrandom(int burst)
{
    static int ofs = 0;
    int rand_value = 1 + (rand_vals[ofs] % burst);
    ofs = (ofs + 1) % rsize;
    return rand_value;
}

void read_input_file(char *filename)
{
    ifstream proc_file(filename);
    string line;
    while (getline(proc_file, line))
    {
        int AT, TC, CB, IO;
        istringstream stream(line);
        if (!(stream >> AT >> TC >> CB >> IO))
            throw std::runtime_error("invalid data");

        if (debug_flag==true)
            cout << "Process Info: " << AT << " " << TC << " " << CB << " " << IO << endl;
        Process curr_proc = Process(AT, TC, CB, IO, maxprio);
        process_list.push_back(curr_proc);
    }

    for (int i = 0; i < process_list.size(); i++)
    {
        Event *event = new Event(&process_list[i], process_list[i].arrival_time, process_list[i].arrival_time, STATE_READY, STATE_CREATED, newEventNumber());
        add_event(event);
    }
    if (debug_flag==true)
        print_event_queue();
}

void print_summary()
{
    int final_finish_time = 0;
    int proc_tat;
    double total_cpu_time = 0, total_tat = 0, total_wait_time = 0;
    double count = double(process_list.size());
    int count2 = 0;
    Process *proc;
    for (int i = 0; i < process_list.size(); i++)
    {
        proc = &process_list[i];
        proc_tat = proc->finish_time - proc->arrival_time;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
               proc->id,
               proc->arrival_time,
               proc->total_cpu_time,
               proc->cpu_burst_lim,
               proc->io_burst_lim,
               proc->static_priority,
               proc->finish_time,
               proc_tat,
               proc->io_time,
               proc->wait_time);

        total_cpu_time += double(proc->total_cpu_time);
        total_tat += proc_tat;
        total_wait_time += proc->wait_time;
        final_finish_time = max(final_finish_time, proc->finish_time);
        count2++;
    }

    double finish_time = double(final_finish_time);
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
           final_finish_time,
           (total_cpu_time / finish_time) * 100.00,
           ((double)total_io_time / finish_time) * 100.00,
           total_tat / count2,
           total_wait_time / count2,
           (count2 * 100) / finish_time);
}