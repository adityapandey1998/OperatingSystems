#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <climits>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <set>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

using namespace std;

map<char, bool> cmdOption;

ifstream input_file;
string line;
int totalFinishTime;
int totalMovement;

struct IORequest {
    int id;
    int arrivalTime;
    int finishTime;
    int startTime;
    int track;
};

IORequest *currentRequest, *nextRequest;
deque<IORequest*> allRequests, IOQueue;
deque<IORequest*> schedQueue;

int currentTrack = 0;
int currentDirection = 1;
int currentTime = 0;

class Scheduler{
    public:
        virtual void addIORequest(IORequest* req) = 0;
        virtual IORequest* getIORequest() = 0;
};

class FIFO: public Scheduler {
    public:
        void addIORequest(IORequest* req) {
            schedQueue.push_back(req);
        };

        IORequest* getIORequest() {
            IORequest* issued = schedQueue.front();
            schedQueue.pop_front();
            return issued;
        }
};

class SSTF: public Scheduler {
    IORequest* currentRequest;
    IORequest* issued;
    int issuedIndex;
    public:
        void addIORequest(IORequest* req) {
            schedQueue.push_back(req);
        };

        IORequest* getIORequest() {
            int minSeek = INT_MAX;
            for(int i=0; i<schedQueue.size(); i++) {
                currentRequest = schedQueue[i];
                if(abs(currentTrack - currentRequest->track)<minSeek) {
                    minSeek = abs(currentTrack - currentRequest->track);
                    issuedIndex=i;
                }

            }
            issued = schedQueue[issuedIndex];
            schedQueue.erase(schedQueue.begin()+issuedIndex);
            return issued;
        }
};

class LOOK: public Scheduler {
    IORequest* currentRequest;
    IORequest* issued;
    public:
        void addIORequest(IORequest* req) {
            schedQueue.push_back(req);
        };

        IORequest* getIORequest_util(deque<IORequest*> &schedQueue) {
            int issuedIndex;
            int minSeek = INT_MAX;
            int currSeek;
            bool found=false;
            for(int i=0; i< schedQueue.size(); i++) {
                currSeek = schedQueue[i]->track - currentTrack;
                int sameDirection = currSeek/currentDirection;
                if(currentDirection==0 || sameDirection>=0) {
                    if( abs(currSeek) < minSeek) {
                        minSeek = abs(currSeek);
                        issuedIndex=i;
                        found=true;
                    }
                }
            }
            
            if(found==true) {
                issued = schedQueue[issuedIndex];
                schedQueue.erase(schedQueue.begin()+issuedIndex);
                return issued;
            } else {
                currentDirection*=-1;
                for(int i=0; i< schedQueue.size(); i++) {
                    currSeek = schedQueue[i]->track - currentTrack;
                    int sameDirection = currSeek/currentDirection;
                    if(currentDirection==0 || sameDirection>=0) {
                        if( abs(currSeek) < minSeek) {
                            minSeek = abs(currSeek);
                            issuedIndex=i;
                            found=true;
                        }
                    }
                }
                
                if(found==true) {
                    issued = schedQueue[issuedIndex];
                    schedQueue.erase(schedQueue.begin()+issuedIndex);
                    return issued;
                } else {
                    return nullptr;
                }
            }
        }
        IORequest* getIORequest() {
            return getIORequest_util(schedQueue);
        }
};

class CLOOK: public Scheduler {
    IORequest* currentRequest;
    IORequest* issued;
    int issuedIndex;
    public:
        void addIORequest(IORequest* req) {
            schedQueue.push_back(req);
        };
        IORequest* getIORequest() {
            int minSeek = INT_MAX;
            int currSeek;
            bool found=false;
            for(int i=0; i< schedQueue.size(); i++) {
                currSeek = schedQueue[i]->track - currentTrack;
                if( currSeek>=0 && abs(currSeek) < minSeek) {
                    minSeek = abs(currSeek);
                    issuedIndex=i;
                    found=true;
                }   
            }
            if(found==true) {
                issued = schedQueue[issuedIndex];
                schedQueue.erase(schedQueue.begin()+issuedIndex);
                return issued;
            } else {
                for(int i=0; i< schedQueue.size(); i++) {
                    currSeek = schedQueue[i]->track;
                    if( abs(currSeek) < minSeek) {
                            minSeek = abs(currSeek);
                            issuedIndex=i;
                            found=true;
                    }
                }
                if(found==true) {
                    issued = schedQueue[issuedIndex];
                    schedQueue.erase(schedQueue.begin()+issuedIndex);
                    return issued;
                } 
            }
            return nullptr;
        }
    
};

class FLOOK: public LOOK {
    deque<IORequest*> *addQueue, *activeQueue, *temp;
    public:
        FLOOK() {
            addQueue = new deque<IORequest*>;
            activeQueue = new deque<IORequest*>;
        }

        void addIORequest(IORequest* req){
            if(::currentRequest==nullptr) {
                activeQueue->push_back(req);   
                return;
            }
            addQueue->push_back(req);
        };

        IORequest* getIORequest() {
            if(activeQueue->empty()) {
                temp = activeQueue;
                activeQueue = addQueue;
                addQueue = temp;
            }
            return getIORequest_util(*activeQueue);
        }
};

string get_next_line() {
  // input_file.open(filename);
  string line;
  do {
    getline(input_file, line);
    if(input_file.eof())
      return "#";
  } while (line[0] == '#' || line.empty());
  return line;
}

void readIO() {
    int id=0, arrivalTime, track;
    string line;
    while((line=get_next_line())[0]!='#') {
        istringstream stream(line);
        stream >> arrivalTime >> track;
        
        IORequest *request = new IORequest;
        request->arrivalTime = arrivalTime;
        request->track = track;
        request->id = id++;
        request->startTime = INT_MIN;
        request->finishTime = INT_MIN;

        allRequests.push_back(request);
        IOQueue.push_back(request);
    }
}

void printSummary() {
    double turnaroundTime = 0.0;
    double totalWaitTime = 0.0;
    int maxWaitTime = INT_MIN;
    double currWaitTime, currTurnaroundTime;

    for(int i=0; i<allRequests.size(); i++) {
        IORequest *currRequest = allRequests[i];
        currWaitTime = currRequest->startTime - currRequest->arrivalTime;
        currTurnaroundTime = currRequest->finishTime - currRequest->arrivalTime;
        turnaroundTime += currTurnaroundTime;
        totalWaitTime += currWaitTime;
        
        printf("%5d: %5d %5d %5d\n",
            currRequest->id, 
            currRequest->arrivalTime, 
            currRequest->startTime, 
            currRequest->finishTime
        );

        if(currWaitTime>maxWaitTime)
            maxWaitTime = currWaitTime;
    }

    printf("SUM: %d %d %.2lf %.2lf %d\n",
        totalFinishTime,
        totalMovement,
        turnaroundTime/double(allRequests.size()),
        totalWaitTime/double(allRequests.size()),
        maxWaitTime
    );
}
void Simulation(map<char, bool> cmdOption);
Scheduler *THE_SCHEDULER;

int main(int argc, char**argv) {
    char c;
    while ((c = getopt(argc, argv, "vqfs:")) != -1)
        switch (c) {
        case 'v': {
            cmdOption['v'] = true;
            break;
        }
        case 'q': {
            cmdOption['q'] = true;
            break;
        }
        case 'f': {
            cmdOption['f'] = true;
            break;
        }
        case 's': {
            char IOSchedType = optarg[0];
            switch (IOSchedType) {
                case 'i':
                    THE_SCHEDULER = new FIFO();
                    break;
                case 'j':
                    THE_SCHEDULER = new SSTF();
                    break;
                case 's':
                    THE_SCHEDULER = new LOOK();
                    break;
                case 'c':
                    THE_SCHEDULER = new CLOOK();
                    break;
                case 'f':
                    THE_SCHEDULER = new FLOOK();
                    break;
                }
            break;
            }
        }
    
    char* input_filename = argv[argc - 1];
    
    input_file.open(input_filename);
    readIO();
    Simulation(cmdOption);
    printSummary();
}

void Simulation(map<char, bool> cmdOption) {
    currentTrack = 0;
    int pending = 0;
    while(true) {
        
        nextRequest = nullptr;
        if(IOQueue.empty()){
            nextRequest = nullptr;
        } else {
            nextRequest = IOQueue.front();
        }

        // if a new I/O arrived to the system at this current time
        //  → add request to IO-queue
        if(nextRequest!=nullptr) {
            if(nextRequest->arrivalTime == currentTime){
                IOQueue.pop_front();
                                
                THE_SCHEDULER->addIORequest(nextRequest);
                pending+=1;
            }
        }

        // if an IO is active
        if(currentRequest!=nullptr) {
            if(currentRequest->track == currentTrack) {
                // if an IO is active and completed at this time
                // → Compute relevant info and store in IO request for final summary
                currentRequest->finishTime = currentTime;
                currentRequest = nullptr;
            } else {
                // if an IO is active
                // → Move the head by one unit in the direction its going (to simulate seek)
                currentTrack += currentDirection;
                totalMovement += 1;
            }
        }

        
        // if no IO request active now
        if(currentRequest == nullptr) {
        
            // if requests are pending
            // → Fetch the next request from IO-queue and start the new IO.
            if(pending>0) {
                pending-=1;
                currentRequest = THE_SCHEDULER->getIORequest();
                currentRequest->startTime = currentTime;
                
                if(currentRequest->track == currentTrack) {
                    // Offset for the general currentTime+=1
                    currentTime -= 1;
                } else {
                    int track_diff = abs(currentRequest->track - currentTrack)/(currentRequest->track - currentTrack);
                    currentDirection = track_diff;
                    currentTrack += currentDirection;
                    totalMovement += 1;
                }
            } 
        }
        // else if all IO from input file processed
        // → exit simulation
        if(currentRequest == nullptr && pending == 0 ) {
            if(IOQueue.empty()) {
                totalFinishTime = currentTime;
                break;
            }
        }
        currentTime+=1;
    }
}
