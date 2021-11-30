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
#include <string.h>

#define MAX_PAGE_COUNT 64
#define MAX_FRAME_COUNT 128

using namespace std;

unsigned long long cost=0;
map<char, bool> cmdOption;
vector<int> rand_vals;
int num_frames;
int rsize;
int process_count=0, context_switches=0, process_exits=0, inst_count=0;
ifstream input_file;
string line;
map<string, unsigned long long> cost_map = {
  { "MAP", 300 },     { "UNMAP", 400 }, { "IN", 3100 },   { "OUT", 2700 },
  { "FIN", 2800 },    { "FOUT", 2400 }, { "ZERO", 140 },  { "SEGV", 340 },
  { "SEGPROT", 420 }, { "ACCESS", 1 },  { "EXIT", 1250 }, { "CTX_SWITCH", 130 }
};

  struct frame_t
{
  unsigned int address : 7;
  unsigned int age : 32;
  unsigned int pid;
  unsigned int vpage;
  long long last_used;
  bool mapped;
};
frame_t frame_table[MAX_FRAME_COUNT];
deque<frame_t*> free_pool;

void init_free_pool() {
  for (int i=0;i<num_frames;i++) {
    free_pool.push_back(&frame_table[i]);
  }
}

void clear_frame(frame_t *frame, int addr, int inst_num) {
    frame->address = addr;
    frame->last_used = inst_num;
    frame->pid = -1;
    frame->vpage = -1;
    frame->mapped = false;
}
void init_frame_table() {
    for (int i=0;i<num_frames; i++) {
        clear_frame(&frame_table[i], i, -1);
    }
}

struct VMA {
    int vpage_start;
    int vpage_end;
    int write_protected;
    int file_mapped;
};

struct pte_t
{
  unsigned int frame_number : 7;
  unsigned int valid : 1;
  unsigned int referenced : 1;
  unsigned int modified : 1;
  unsigned int write_protected : 1;
  unsigned int paged_out : 1;
  unsigned int is_mapped : 1;
  unsigned int padding : 19;
};

void clear_pte(pte_t *curr, bool paged_out) {
    curr->frame_number = 0;
    curr->valid = 0;
    curr->referenced = 0;
    curr->modified = 0;
    curr->write_protected = 0;
    curr->is_mapped = 0;
    if (paged_out)
        curr->paged_out = 0;
}

class Process{
    public:
        int id;
        vector<VMA> VMAs;
        pte_t page_table[MAX_PAGE_COUNT];

        long long unmaps = 0;
        long long maps = 0;
        long long ins = 0;
        long long outs = 0;
        long long fins = 0;
        long long fouts = 0;
        long long zeros = 0;
        long long segv = 0;
        long long segprot = 0;

        Process(int id) {
            this->id=id;
            for(int i=0; i<MAX_PAGE_COUNT;i++)
                clear_pte(&page_table[i], true);
        }
        VMA* get_vma(int vpage) {
            for(int i=0; i<VMAs.size(); i++) {
                if(vpage>=VMAs[i].vpage_start && vpage<=VMAs[i].vpage_end)
                    return &VMAs[i];
            }
            return nullptr;
        }
        void empty_page_table() {
            for(int i=0; i<MAX_PAGE_COUNT;i++)
                clear_pte(&page_table[i], true);
        }
        void add_vma(VMA vma) {
            VMAs.push_back(vma);
        }
        pte_t get_pte(int vpage) {
            return page_table[vpage];
        }
        void set_pte(int vpage, pte_t curr) {
            // cout<<"Setting pte_t with valid="<<curr.valid<<endl;
            page_table[vpage]=curr;
            // cout<<"page_table[]"<<vpage<<" "<<page_table[vpage].valid<<endl;
        }
};
vector <Process*> process_list;
Process *current_process;
    
string get_next_line()
{
  // input_file.open(filename);
  string line;
  do {
    getline(input_file, line);
    if(input_file.eof())
      return "#";
  } while (line[0] == '#' || line.empty());
  return line;
}

int get_next_instruction(char &op, int &vpage) {
    if(line[0]=='#' || input_file.eof())
        return 0;
    // cout << line << endl;
    line=get_next_line();
    if(line[0]=='#' || input_file.eof())
        return 0;
    istringstream stream(line);
    char instruction;
    int pagenum;
    stream >> instruction >> pagenum;
    op = instruction;
    vpage = pagenum;
    return 1;
}


void read_input_file()
{
  int VMA_count = 0;

  process_count = atoi(&(get_next_line()[0]));

  for (int i = 0; i < process_count; i++) {
    Process* proc = new Process(i);
    
    int VMA_count = atoi(&(get_next_line()[0]));

    for (int j = 0; j < VMA_count; j++) {
        int vpage_start, vpage_end, write_protected, file_mapped;
        line = get_next_line();
        istringstream stream(line);
        stream >> vpage_start >> vpage_end >> write_protected >> file_mapped;
        VMA vma; 
        vma.vpage_start = vpage_start;
        vma.vpage_end = vpage_end;
        vma.write_protected = write_protected;
        vma.file_mapped = file_mapped;
        proc->add_vma(vma);
    }
    process_list.push_back(proc);
  }
}

void read_random_file(string filename)
{
  int size, n;
  ifstream rand_file(filename);
  rand_file >> size;
  rsize = size;
  while (rand_file >> n)
    rand_vals.push_back(n);
}

int myrandom(int limit)
{
  static int ofs = 0;
  int rand_value = (rand_vals[ofs] % limit);
  ofs = (ofs + 1) % rsize;
  return rand_value;
}

class Pager
{
public:
  int frame_count=0;
  virtual frame_t* select_victim_frame() = 0;
};
Pager* THE_PAGER;

class FIFO: public Pager {
  public:
    frame_t* select_victim_frame() {
      frame_t *victim = &frame_table[frame_count];
      frame_count = (frame_count+1)%(num_frames);
      return victim;
    }
};

class Random: public Pager {
  public:
      frame_t* select_victim_frame() {
        int index = myrandom(num_frames);
        return &frame_table[index];
      }
};

class Clock: public Pager {
      frame_t curr_frame;
      Process *curr_process;
      pte_t curr_pte;
  public:
    frame_t* select_victim_frame() {
      frame_count = frame_count%num_frames;
      for (int i=0; i<num_frames;i++) {
        int ind = (i+frame_count)%num_frames;
        curr_frame = frame_table[ind];

        curr_pte = process_list[curr_frame.pid]->get_pte(curr_frame.vpage);
        if(curr_pte.referenced == 0) {
          // cout<<"ASELECT"<<frame_count<< " " << temp+1 << endl;
          frame_count = (ind+1);
          return &frame_table[ind];
        }
        curr_pte.referenced=0;
        process_list[curr_frame.pid]->set_pte(curr_frame.vpage, curr_pte);
      }
      int return_index = frame_count;
      frame_count = frame_count+1;
      return &frame_table[return_index];
    }
};

class Aging: public Pager {
  frame_t curr_frame;
  pte_t curr_pte;
  int frame_index;
  public:
    frame_t* select_victim_frame() {
      frame_count%=num_frames;
      unsigned long long min_age = -1;
      for (int i=0; i<num_frames;i++) {
        int ind = (i+frame_count)%num_frames;
        curr_frame = frame_table[ind];

        curr_pte = process_list[curr_frame.pid]->get_pte(curr_frame.vpage);
        curr_frame.age = curr_frame.age >> 1;
        if(curr_pte.referenced) 
          curr_frame.age = curr_frame.age | 0x80000000;
        
        curr_pte.referenced=0;
        process_list[curr_frame.pid]->set_pte(curr_frame.vpage, curr_pte);

        if(curr_frame.age < min_age) {
          frame_index = ind;
          min_age=curr_frame.age;
        }
        frame_table[ind] = curr_frame;
      }
      frame_count = frame_index+1;
      return &frame_table[frame_index];
    }
};

class WorkingSet: public Pager {
  frame_t curr_frame;
  pte_t curr_pte;
  public:
    frame_t* select_victim_frame() {
      frame_count = frame_count%num_frames;
      int frame_index=frame_count;
      unsigned long long age=0;

      for (int i=0; i<num_frames; i++) {
        int ind = (i+frame_count)%num_frames;
        curr_frame = frame_table[ind];
        curr_pte = process_list[curr_frame.pid]->get_pte(curr_frame.vpage);
        int curr_age = inst_count-curr_frame.last_used;
  
        if(!curr_pte.referenced && curr_age>=50) {
          frame_index=ind;
          break;
        }

        if(curr_pte.referenced) {
          curr_frame.last_used=inst_count;
          curr_pte.referenced=0;
        } else if(curr_age>age) {
          age=curr_age;
          frame_index=ind;
        }
        process_list[curr_frame.pid]->set_pte(curr_frame.vpage, curr_pte);
        frame_table[ind]=curr_frame;
      }
      frame_count=frame_index+1;
      return &frame_table[frame_index];
    }
};

class NRU: public Pager {
  frame_t curr_frame;
  Process *curr_process;
  pte_t curr_pte;
  int prev_inst_count=0;
  public:
    frame_t* select_victim_frame() {
      frame_count = frame_count%num_frames;
      int frame_index=frame_count;
      unsigned long long age=0;

      int min_class_ind=4;
      for (int i=0; i<num_frames; i++) {
        int ind = (i+frame_count)%num_frames;
        curr_frame = frame_table[ind];
        curr_pte = process_list[curr_frame.pid]->get_pte(curr_frame.vpage);
        int class_ind = 2*curr_pte.referenced + curr_pte.modified;

        if(class_ind<min_class_ind) {
          min_class_ind=class_ind;
          frame_index=ind;
          if(min_class_ind==0)
            break;
        }
      }
      int curr_age = inst_count-prev_inst_count;
      if(curr_age+1>=50) {
        prev_inst_count = inst_count+1;
        for(int i=0;i<process_list.size(); i++) {
          for(int j=0; j<MAX_PAGE_COUNT; j++) {
            curr_pte=process_list[i]->get_pte(j);
            curr_pte.referenced=0;
            process_list[i]->set_pte(j, curr_pte);
          }
        }
      }
      
      frame_count=frame_index+1;
      return &frame_table[frame_index];

    }
};

frame_t* get_new_frame() {
  if(free_pool.size() == 0) {
    return THE_PAGER->select_victim_frame();
  } else {
    frame_t* front = free_pool.front();
    free_pool.pop_front();
    return front;
  }
}

void Simulation(map<char, bool> cmdOption)
{
    init_frame_table();
    init_free_pool();
    // cout<<"In Simulation" << endl;
    char op;
    int vpage;
    int current_process_index=-1;
    while(get_next_instruction(op, vpage)) {
      // if (cmdOption['D']) 
      if(cmdOption['O'])
        cout<<inst_count<<": ==> " << op << " " << vpage <<endl;
      if (op=='c') {
        current_process = process_list[vpage];
        context_switches++;
        current_process_index=vpage;
        cost += cost_map["CTX_SWITCH"];
      } 
      else if (op=='e') {
        if(cmdOption['O'])
          cout<<"EXIT current process "<<current_process->id<<endl;
        process_exits++;
        cost+= cost_map["EXIT"];
        pte_t temp;
        for(int i=0;i<MAX_PAGE_COUNT; i++) {
          temp = current_process->get_pte(i);
          if(temp.valid) {
            int frame_num = temp.frame_number;
            frame_t frame = frame_table[frame_num];
            cost+=cost_map["UNMAP"];
            current_process->unmaps+=1;
            
            // PRINT UNMAP DETAILS
            if(cmdOption['O'])
              cout<<" UNMAP " << frame.pid << ":"<<frame.vpage<<endl;
            
            VMA *currVMA = current_process->get_vma(i);
            if(currVMA->file_mapped) {
              if (temp.modified) {
            
                // Print O
                if(cmdOption['O'])
                  cout<<" FOUT"<<endl;
            
                cost+=cost_map["FOUT"];
                current_process->fouts+=1;
              }
            }
            clear_frame(&frame, frame_num, -1);
            frame_table[frame_num]=frame;
            free_pool.push_back(&frame_table[frame_num]);
            clear_pte(&temp, true);
          }
          current_process->set_pte(i, temp);
        }
        current_process->empty_page_table();
        current_process=nullptr;
      }
      else {
        cost+=cost_map["ACCESS"];
        pte_t curr_pte = current_process->page_table[vpage];
        // If the page table entry is already there, we do not need to do anything different
        if(!curr_pte.valid) 
        {
          VMA *curr_vma = current_process->get_vma(vpage);
          if(curr_vma==nullptr) {
            if(cmdOption['O'])
              cout<<" SEGV"<<endl;
            cost+=cost_map["SEGV"];
            inst_count+=1;
            current_process->segv+=1;
            continue;
          } else {
            curr_pte.is_mapped=curr_vma->file_mapped;
            curr_pte.write_protected=curr_vma->write_protected;
            
            frame_t *next_frame = get_new_frame();

            if (next_frame->mapped) {
              cost+=cost_map["UNMAP"];
              int old_vpage = next_frame->vpage;
              int old_pid = next_frame->pid;
              Process *old_process = process_list[old_pid];
              pte_t old_pte = old_process->get_pte(old_vpage);

              if(cmdOption['O'])
                cout << " UNMAP " << old_pid << ":" << old_vpage << endl;
              old_process->unmaps+=1;

              if(old_pte.modified) {
                if (old_pte.is_mapped) {
                  if(cmdOption['O'])
                    cout << " FOUT" << endl;
                  cost+=cost_map["FOUT"];
                  old_process->fouts+=1;
                } else {
                  if(cmdOption['O'])
                    cout << " OUT" << endl;
                  cost+=cost_map["OUT"];
                  old_process->outs+=1;
                  old_pte.paged_out=1;
                }
              }

              // cout<<"Clearing pte_t" <<endl;
              clear_pte(&old_pte, false);
              // cout<<old_pte.valid<<endl;

              old_process->set_pte(old_vpage, old_pte);
              clear_frame(next_frame, next_frame->address, inst_count);
              // cout<< "PT[" << old_process->id<<"]: "<< vpage<<": "<<old_process->page_table[vpage].valid<<endl;
            }
            next_frame->pid = current_process->id;
            next_frame->mapped=1;
            next_frame->vpage=vpage;
            curr_pte.frame_number = next_frame->address;
            
            if(curr_pte.paged_out) {
              if(cmdOption['O'])
                cout <<" IN"<<endl;
              cost+=cost_map["IN"];
              current_process->ins+=1;
            } else if(curr_pte.is_mapped) {
              if(cmdOption['O'])
                cout <<" FIN"<<endl;
              cost+=cost_map["FIN"];
              current_process->fins+=1;
            } else {
              if(cmdOption['O'])
                cout <<" ZERO"<<endl;
              cost+=cost_map["ZERO"];
              current_process->zeros+=1;
            }

            
            cost+=cost_map["MAP"];
            if(cmdOption['O'])
              cout<< " MAP "<<next_frame->address<<endl;
            next_frame->last_used=inst_count;
            
            next_frame->age=0;
            current_process->maps+=1;
          }
        }
        curr_pte.referenced=1;
        curr_pte.valid=1;

        if(op=='w') {
          if(curr_pte.write_protected) {
            if(cmdOption['O'])
              cout << " SEGPROT" <<endl;
            current_process->segprot+=1;
            cost+=cost_map["SEGPROT"];
          } else {
            curr_pte.modified=1;
          }
        }
        current_process->page_table[vpage]=curr_pte;

      }
      inst_count+=1;
    }
}

void print_output(map<char, bool> cmdOption) {

  // P Flag
  if(cmdOption['P']){
    Process *curr;
    int curr_pid;
    for(int i=0; i<process_list.size(); i++) {
      curr = process_list[i];
      int curr_pid = curr->id;
      cout << "PT[" << curr_pid <<"]:";
      for(int j=0; j<MAX_PAGE_COUNT; j++) {
        // cout << "PT[" << process_list[i]->id<<"]: "<< j<<": "<<process_list[i]->page_table[j].valid<<endl;
        
        cout<<" ";
        pte_t curr_pte = curr->page_table[j];
        if(curr->page_table[j].valid==1) {
          cout<< j << ":";
          if(curr_pte.referenced) 
            cout<<"R";
          else 
            cout<<"-";

          if(curr_pte.modified) 
            cout<<"M";
          else 
            cout<<"-";

          if(curr_pte.paged_out) 
            cout<<"S";
          else 
            cout<<"-";
        } else if (curr_pte.paged_out) {
          cout<<"#";
        } else {
          cout<<"*";
        }
      }
      cout<<endl;
    }
  }
  
  // F Flag
  if(cmdOption['F']) {
      cout<<"FT:";
    frame_t f;
    for(int i=0;i<num_frames;i++) {
      f=frame_table[i];
      if(f.mapped) {
        cout<< " " << f.pid << ":" << f.vpage;
      } else {
        cout<<" *";
      }
    }
    cout<<endl;
  }

  // S Flag
  if(cmdOption['S']) {
    Process *curr;
    for(int i=0; i<process_list.size(); i++) {
    curr=process_list[i];
    cout<< "PROC[" << curr->id << "]: U=" << curr->unmaps << " M=" << curr->maps;
    cout<< " I=" << curr->ins << " O=" << curr->outs << " FI=" << curr->fins << " FO=" << curr->fouts;
    cout<< " Z=" << curr->zeros << " SV=" << curr->segv << " SP=" << curr->segprot << endl;
  }

  cout << "TOTALCOST " << inst_count << " " << context_switches << " " << process_exits << " " << cost << " " << sizeof(pte_t) << endl;
  }
}

int main(int argc, char** argv)
{
  char c;
  while ((c = getopt(argc, argv, "o:f:a:")) != -1)
    switch (c) {
      case 'f':
        num_frames = atoi(optarg);
        break;
      case 'o': {
        char* option_o = optarg;
        cmdOption['O'] = true;
        cmdOption['P'] = true;
        cmdOption['F'] = true;
        cmdOption['S'] = true;
        if (strlen(option_o) > 4) {
          cmdOption['D'] = true;
        }
      }
      case 'a': {
        char pager_type = optarg[0];
        switch (pager_type) {
          case 'f':
            THE_PAGER = new FIFO();
            break;
          case 'c':
            THE_PAGER = new Clock();
            break;
          case 'r':
            THE_PAGER = new Random();
            break;
          case 'e':
            THE_PAGER = new NRU();
            break;
          case 'a':
            THE_PAGER = new Aging();
            break;
          case 'w':
            THE_PAGER = new WorkingSet();
            break;
        }
      }
    }
  char* input_filename = argv[argc - 2];
  char* rand_filename = argv[argc - 1];

  input_file.open(input_filename);
  read_input_file();
  read_random_file(rand_filename);
  Simulation(cmdOption);
  print_output(cmdOption);
}