#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <getopt.h>
#include <vector>
#include <queue>
#include <climits>

using namespace std;
#define BUFFER_SIZE 1024

typedef enum { STATE_RUNNING , STATE_BLOCKED , STATE_CREATED, STATE_READY, STATE_DONE, STATE_PREEMPT } process_state_t;
string STATE_NAMES[] = {"RUNNG", "BLOCK", "CREATED", "READY", "BLOCK", "PREEMPT"};

int ofs = 0;
int RAND_LENGTH;
bool vFlag=false;
bool tFlag=false;
bool eFlag=false;
int quantum=10000;
int maxprio = 4;
char schedOption;
int totalIOTime = 0;
int IOProcesses = 0;
int IOStintStartTime = 0;
string inputFileName;
string randFileName;
vector<int> randvals;

class Process {
   	public:
		int pid;
		int arrivalTime;
		int totalCPUTime;
		int remCPUTime;
		int CPUBurst;
		int IOBurst;
		int static_prio;
		int dynamic_prio;
		int IOTime;
		int CPUWaitingTime;
		int finishingTime;
		int readyQTS;
		bool preempted;
		int remCPUBurst;
		Process(int id, int at, int tc, int cb, int io, int priority){
			pid = id;
			arrivalTime = at;
			totalCPUTime = tc;
			remCPUTime = tc;
			CPUBurst = cb;
			IOBurst = io;
			static_prio = priority;
			dynamic_prio = static_prio - 1;
			IOTime = 0;
			CPUWaitingTime = 0;
			preempted = false;
		}
};

class Event {
   	public:
        int timestamp;
		int lastTimestamp;
		Process *process;
        process_state_t state;
		process_state_t prevState;

		Event(int ts, int lastTs, Process *p, process_state_t trans_state, process_state_t prev_trans_state){
			timestamp = ts;
			lastTimestamp = lastTs;
			process = p;
			state = trans_state;
			prevState = prev_trans_state;
		}
};

deque<Event*> event_queue;
deque<Process*> run_queue;
deque<Process*> *activeQ;
deque<Process*> *expiredQ;
void print_activeQ(deque<Process*> *parentQ);
void print_event_queue(bool state);
void print_add_event_queue(Event *newEvt);
void print_expired_and_run_queue();
void print_run_queue_non_prio(bool rev);
deque<Process*> process_list;
Process *CURRENT_RUNNING_PROCESS = nullptr;


Event* get_event(){
	if(event_queue.empty())
		return nullptr;

    Event *e = event_queue.front();
    event_queue.pop_front();
    return e;
}

void put_event(Event *newEvent){
    int index = 0;
    while(index < event_queue.size()){
		Event *event = event_queue[index];
		if(event->timestamp > newEvent->timestamp)
			break;
		index++;
	}
	event_queue.insert(event_queue.begin()+index, newEvent);
}

class Scheduler{
    public:
		string name;
		Scheduler() {};
        virtual void add_process(Process *p) {
			run_queue.push_back(p);
			p->dynamic_prio = p->static_prio - 1;
		};
		virtual Process* get_next_process(){
			return nullptr;
		}
		void test_preempt(Process *p, int current_time);
		virtual ~Scheduler() {};
};


class LCFS: public Scheduler{
	public:
		LCFS() {
			name = "LCFS";
		};

		Process* get_next_process(){
			if(vFlag && tFlag)
				print_run_queue_non_prio(true);
			if(run_queue.empty())
				return nullptr;
			Process *p1 = run_queue.back();
			run_queue.pop_back();
			return p1;
		}
		virtual ~LCFS() {};
};


class FCFS: public Scheduler{
	public:
		FCFS() {
			name = "FCFS";
		};

		Process* get_next_process(){
			if(vFlag && tFlag)
				print_run_queue_non_prio(false);
			if(run_queue.empty())
				return nullptr;
			Process *p1 = run_queue.front();
			run_queue.pop_front();
			return p1;
		}
		virtual ~FCFS() {};
};


class SRTF: public FCFS{
	public:
		SRTF() {
			name = "SRTF";
		};

        void add_process(Process *p) {

			int index = 0;
			while(index<run_queue.size()){
				Process *p1 = run_queue[index];
				if(p1->remCPUTime > p->remCPUTime)
					break;
				index++;
			}
			run_queue.insert(run_queue.begin()+index, p);
			p->dynamic_prio = p->static_prio - 1;
		}
		
		virtual ~SRTF() {};
};


class RR: public FCFS{
	public:
		RR(int q) {
			name = "RR "+to_string(q);
		};
		virtual ~RR() {};
};

class Prio: public Scheduler{
	public:
		Prio() {}
		Prio(int q){
			name = "PRIO "+to_string(q);
			activeQ = new deque<Process*>[maxprio];
			expiredQ = new deque<Process*>[maxprio];
		}
        void add_process(Process *p) {
			if(p->dynamic_prio == -1){
				p->dynamic_prio = p->static_prio - 1;
				expiredQ[p->dynamic_prio].push_back(p);
			}
			else{
				activeQ[p->dynamic_prio].push_back(p);
			}
		}
		Process* get_next_process(){
			if(vFlag && tFlag)
				print_expired_and_run_queue();
			deque<Process*> *queue;
			for(int i=maxprio-1;i>=0;i--){
        		queue = &activeQ[i];
				if(!queue->empty()){
					Process *p1 = queue->front();
					queue->pop_front();
					return p1;
				}
			}
			if(vFlag && tFlag)
				cout<<"switched queues\n";
			queue = activeQ;
			activeQ = expiredQ;
			expiredQ = queue;
			for(int i=maxprio-1;i>=0;i--){
        		queue = &activeQ[i];
				if(!queue->empty()){
					Process *p1 = queue->front();
					queue->pop_front();
					return p1;
				}
			}
			return nullptr;
		}
		virtual ~Prio() {};

};
class PrePrio: public Prio{
	public:
		PrePrio(int q){
			name = "PREPRIO "+to_string(q);
			activeQ = new deque<Process*>[maxprio];
			expiredQ = new deque<Process*>[maxprio];
		}
		int current_process_event(){
			for (Event *e1: event_queue){
				if(e1->process == CURRENT_RUNNING_PROCESS)
					return e1->timestamp;
			}
			return -1;
		}
		int rm_event(){
			Event *e1;
			int index = 0;
			while (index < event_queue.size()){
				e1 = event_queue[index];
				if(e1->process == CURRENT_RUNNING_PROCESS){
					int res = e1->lastTimestamp;
					event_queue.erase(event_queue.begin()+index);
					return res;
				}
				index++;
			}
			return -1;
		}
        void add_process(Process *p) {
			if(p->dynamic_prio == -1){
				p->dynamic_prio = p->static_prio - 1;
				expiredQ[p->dynamic_prio].push_back(p);
			}
			else if(CURRENT_RUNNING_PROCESS != nullptr){
				bool prioGreater = p->dynamic_prio>CURRENT_RUNNING_PROCESS->dynamic_prio;
				int current_process_event_ts = current_process_event();
				bool timestampEq = current_process_event_ts != p->readyQTS;
				if(vFlag){
					cout<<"---> PRIO preemption "<<CURRENT_RUNNING_PROCESS->pid<<" by "<<p->pid<<" ? "<<prioGreater<<
					" TS="<<current_process_event_ts<<" now="<<p->readyQTS<<") --> ";
				}
				if(prioGreater && timestampEq){
					// RemoveEvent(0):  48:0  54:1  1000:3 ==>  54:1:READY 1000:3:READY
					if(vFlag){
						cout<<"YES\n";
						if(eFlag){
							cout<<"RemoveEvent("<<CURRENT_RUNNING_PROCESS->pid<<"):";
							print_event_queue(false);
						}
					}
					int prev_ts = rm_event();
					activeQ[p->dynamic_prio].push_back(p);
					Event *newEvt = new Event(p->readyQTS, prev_ts, CURRENT_RUNNING_PROCESS, STATE_PREEMPT, STATE_RUNNING);
					if(vFlag && eFlag){
						cout<<" ==> ";
						print_event_queue(true);
						cout<<"\n";
						print_add_event_queue(newEvt);
					}
					put_event(newEvt);
					if(vFlag && eFlag){
						print_event_queue(true);
						cout<<"\n";
					}
				}
				else{
					if(vFlag)
						cout<<"NO\n";
					activeQ[p->dynamic_prio].push_back(p);
				}

			}
			else
				activeQ[p->dynamic_prio].push_back(p);
		}
		virtual ~PrePrio() {};
};

Scheduler* sched;

int myrandom(int burst) {
    int random = 1 + (randvals[ ofs++ ] % burst);
	if(ofs == RAND_LENGTH)
		ofs = 0;
	return random;
}

void getRandom() {
    ifstream inFile;
    inFile.open(randFileName.c_str());
    if (inFile.is_open())
    {
		inFile>>RAND_LENGTH;
		int x;
        for (int i = 0; i < RAND_LENGTH; i++){
			inFile >> x;
			randvals.push_back(x);
		}
        inFile.close();
    }
}



void readProcesses() {
	ifstream inFile;
	inFile.open(inputFileName.c_str());
	int timestamp;
	int totalCPUTime;
	int CPUBurst;
	int IOBurst;
	int pid = 0;
	while(!inFile.eof()){
		timestamp = totalCPUTime = CPUBurst = IOBurst = -1;
		inFile >> timestamp >> totalCPUTime >> CPUBurst >> IOBurst;
		if(timestamp==-1)
			break;
		// cout<<timestamp<<"\t"<<totalCPUTime<<"\t"<<CPUBurst<<"\t"<<IOBurst<<"\n";
		Process *process = new Process(pid, timestamp, totalCPUTime, CPUBurst, IOBurst, myrandom(maxprio));
		Event *event = new Event(timestamp, timestamp, process, STATE_READY, STATE_CREATED);
		process_list.push_back(process);
		put_event(event);
		pid++;
	}
}

void print_deque(){
	cout<<"Printing event\n";
	for (Event *e1: event_queue)
        cout<<e1->timestamp<<"\t"<<e1->lastTimestamp<<"\n"<<
		e1->process->pid<<"\t"<<e1->process->arrivalTime<<"\t"<<
		e1->process->totalCPUTime<<"\t"<<e1->process->CPUBurst<<
		"\t"<<e1->process->IOBurst<<"\t"<<e1->process->dynamic_prio<<
		"\n"<<e1->state<<e1->prevState<<"\n\n";
	cout<<"Done with print deque\n";
}
deque<Process*> *activeQ1;
void print_activeQ(deque<Process*> *parentQ){
    for(int i=0;i<maxprio;i++){
        activeQ1 = &parentQ[i];
        cout<<"Printing event\t"<<i<<"\n";
        for (Process *e1: *activeQ1)
            cout<<e1->pid<<"\n\n";
        cout<<"Done with print deque\n";
    }
}

void print_summary(){
	cout<<sched->name<<"\n";
	int finishingTime = -1;
	int CPUTime = 0;
	double turnAroundTime = 0;
	double CPUWaitingTime = 0;
	double counter = 0;
	for (Process *p1: process_list){
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",p1->pid, p1->arrivalTime, p1->totalCPUTime, p1->CPUBurst, p1->IOBurst, p1->static_prio, p1->finishingTime,p1->finishingTime - p1->arrivalTime, p1->IOTime, p1->CPUWaitingTime);
		CPUTime += p1->totalCPUTime;
		turnAroundTime += p1->finishingTime - p1->arrivalTime;
		CPUWaitingTime += p1->CPUWaitingTime;
		counter++;
		if(finishingTime<p1->finishingTime)
			finishingTime=p1->finishingTime;
	}
	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",finishingTime, 100.0*((double)CPUTime/finishingTime), 100.0*((double)totalIOTime/finishingTime),turnAroundTime/counter,CPUWaitingTime/counter,(counter*100)/finishingTime);

}
int get_next_event_time(){
	if(event_queue.empty())
		return -1;
	Event *evt = event_queue.front();
	return evt->timestamp;
	delete evt;
	evt = nullptr;
}

void print_event_queue(bool state){
	for (Event *e1: event_queue){
		cout<<"  "<<e1->timestamp<<":"<<e1->process->pid;
		if(state)
			cout<<":"<<STATE_NAMES[e1->state];
	}
}

void print_run_queue(bool reverse){
	if(!reverse)
		for (Process *p1: run_queue)
			cout<<" "<<p1->pid<<":"<<p1->readyQTS;
	else{
		Process *p1;
		for (auto it = run_queue.rbegin(); it != run_queue.rend(); ++it){
			p1 = *it;
			cout<<" "<<p1->pid<<":"<<p1->readyQTS;
		}
	}
}

void print_run_queue_non_prio(bool reverse){
	cout<<"SCHED ("<<run_queue.size()<<"): ";
	print_run_queue(reverse);
	cout<<"\n";
}

void print_add_event_queue(Event *newEvt){
	cout<<"  AddEvent("<<newEvt->timestamp<<":"<<newEvt->process->pid<<":"<<STATE_NAMES[newEvt->state]<<"):";
	print_event_queue(true);
	cout<<" ==> ";
}
void print_prio_level(deque<Process*> *queue){
	cout<<"{ ";
	deque<Process*> *tempQ;
	for(int i =maxprio-1;i>=0;i--){
		tempQ = &queue[i];
		cout<<"[";
		bool first = true;
		for(Process *p1:*tempQ){
			if(first){
				cout<<p1->pid;
				first = false;
				continue;
			}
			cout<<","<<p1->pid;
		}
		cout<<"]";
	}
	cout<<"}";
}
void print_expired_and_run_queue(){
	print_prio_level(activeQ);
	cout<<" : ";
	print_prio_level(expiredQ);
	cout<<" : \n";
}

void simulation() {
	Event* evt;
	int current_time;
	int time_in_prev_state;
	bool CALL_SCHEDULER = false;
	if(vFlag && eFlag){
		cout<<"ShowEventQ:";
		print_event_queue(false);
		cout<<"\n";
	}
	while((evt = get_event())!= nullptr){
		Process *proc = evt->process;
		current_time = evt->timestamp;
		time_in_prev_state = current_time - evt -> lastTimestamp;

		switch(evt->state){
			case STATE_READY:{
				if(vFlag == true){
					cout<<current_time<<" "<<evt->process->pid<<" "<<time_in_prev_state<<": "<<STATE_NAMES[evt->prevState]<<" -> "<<STATE_NAMES[evt->state]<<"\n";
				}
				if(evt->prevState == STATE_BLOCKED){
					IOProcesses--;
					if(IOProcesses == 0)
						totalIOTime += current_time - IOStintStartTime;
					proc->IOTime += time_in_prev_state;
				}
				proc->dynamic_prio = proc->static_prio - 1;
				CALL_SCHEDULER = true;
				proc->readyQTS = current_time;
				sched->add_process(proc);
				break;
			}
			case STATE_RUNNING:{
				proc->CPUWaitingTime += time_in_prev_state;
				int CPUBurst;
				if(proc->preempted && proc->remCPUBurst != 0){
					CPUBurst = proc->remCPUBurst;
				}
				else{
					CPUBurst = myrandom(proc->CPUBurst);
					if(CPUBurst>=proc->remCPUTime)
						CPUBurst = proc->remCPUTime;
					proc->remCPUBurst = CPUBurst;
				}

				Event *newEvt;
				if(CPUBurst > quantum){
					newEvt = new Event(current_time+quantum, current_time, proc, STATE_PREEMPT, STATE_RUNNING);
				}
				else if(CPUBurst == proc->remCPUTime){
					newEvt = new Event(current_time + CPUBurst, current_time, proc, STATE_DONE, STATE_RUNNING);
				}
				else{
					newEvt = new Event(current_time + CPUBurst, current_time, proc, STATE_BLOCKED, STATE_RUNNING);
				}
				if(vFlag == true){
					cout<<current_time<<" "<<evt->process->pid<<" "<<time_in_prev_state<<": "<<STATE_NAMES[evt->prevState]<<" -> "<<STATE_NAMES[evt->state]<<" cb="<<CPUBurst<<" rem="<<proc->remCPUTime<<" prio="<<proc->dynamic_prio<<"\n";
					if(eFlag){
						print_add_event_queue(newEvt);
					}
				}
				put_event(newEvt);
				if(vFlag && eFlag){
					print_event_queue(true);
					cout<<"\n";
				}
				break;
			}
			case STATE_BLOCKED:{
				proc->preempted = false;
				CURRENT_RUNNING_PROCESS = nullptr;
				if(IOProcesses == 0){
					IOStintStartTime = current_time;
				}
				IOProcesses++;
				int IOBurst = myrandom(proc->IOBurst);
				proc->remCPUTime -= time_in_prev_state;
				Event *newEvt = new Event(current_time + IOBurst, current_time, proc, STATE_READY, STATE_BLOCKED);
				if(vFlag == true){
					cout<<current_time<<" "<<evt->process->pid<<" "<<time_in_prev_state<<": "<<STATE_NAMES[evt->prevState]<<" -> "<<STATE_NAMES[evt->state]<<"  ib="<<IOBurst<<" rem="<<proc->remCPUTime<<"\n";
					if(eFlag){
						print_add_event_queue(newEvt);
					}
				}
				put_event(newEvt);
				if(vFlag && eFlag){
					print_event_queue(true);
					cout<<"\n";
				}
				CALL_SCHEDULER = true;
				break;
			}
			case STATE_CREATED:{
				break;
			}
			case STATE_DONE:{
				CURRENT_RUNNING_PROCESS = nullptr;
				CALL_SCHEDULER = true;
				proc->finishingTime = current_time;
				if(vFlag == true){
					cout<<current_time<<" "<<evt->process->pid<<" "<<time_in_prev_state<<": Done\n";
				}
				break;
			}
			case STATE_PREEMPT:{ //running to ready
				proc->preempted = true;
				CURRENT_RUNNING_PROCESS = nullptr;
				proc->remCPUTime -= time_in_prev_state;
				proc->remCPUBurst -= time_in_prev_state;
				CALL_SCHEDULER = true;
				if(vFlag){
					cout<<current_time<<" "<<evt->process->pid<<" "<<time_in_prev_state<<": "<<STATE_NAMES[STATE_RUNNING]<<" -> "<<STATE_NAMES[STATE_READY]<<"  cb="<<proc->remCPUBurst<<" rem="<<proc->remCPUTime<<" prio="<<proc->dynamic_prio<<"\n";
				}
				proc->dynamic_prio-=1;
				proc->readyQTS = current_time;
				sched->add_process(proc);
				break;
			}
		}
		delete evt;
		evt = nullptr;
		if(CALL_SCHEDULER == true){
			if (get_next_event_time() == current_time)
				continue;
			CALL_SCHEDULER = false;
			if (CURRENT_RUNNING_PROCESS == nullptr){
				CURRENT_RUNNING_PROCESS = sched->get_next_process();
				if(CURRENT_RUNNING_PROCESS == nullptr)
					continue;
				Event *newEvt = new Event(current_time, CURRENT_RUNNING_PROCESS->readyQTS, CURRENT_RUNNING_PROCESS, STATE_RUNNING, STATE_READY);
				if(vFlag && eFlag){
					print_add_event_queue(newEvt);
				}
				put_event(newEvt);
				if(vFlag && eFlag){
					print_event_queue(true);
					cout<<"\n";
				}
			}
		}
	}
}

int main (int argc, char **argv) {
	int c;
	while ((c = getopt(argc,argv,"vtes:")) != -1 ){
		switch(c) {
			case 'v':{
				vFlag = true;
				break;
			}
			case 't':{
				tFlag = true;
				break;
			}
			case 'e':{
				eFlag = true;
				break;
			}
			case 's':{
				schedOption = optarg[0];
				switch(schedOption){
					case 'F':{
						sched = new FCFS();
						break;
					}
					case 'L':{
						sched = new LCFS();
						break;
					}
					case 'S':{
						sched = new SRTF();
						break;
					}
					case 'R':{
						quantum = atoi(optarg+1);
						sched = new RR(quantum);
						break;
					}
					case 'P':{
						sscanf(optarg+1,"%d:%d",&quantum,&maxprio);
						sched = new Prio(quantum);
						break;
					}
					case 'E':{
						sscanf(optarg+1,"%d:%d",&quantum,&maxprio);
						sched = new PrePrio(quantum);
						break;
					}
				}
				break;
			}
		}

	}
	inputFileName = argv[optind];
	randFileName = argv[optind+1];
	Event *evt;

	getRandom();
	readProcesses();
	// print_deque();
	simulation();
	print_summary();

	run_queue.clear();
	event_queue.clear();
	process_list.clear();
}
