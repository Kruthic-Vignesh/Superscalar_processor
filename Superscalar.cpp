/*  
    CS2610 Assignment 8: SuperScalar Processor
    Authors: Aditya C (CS20B003) & Kruthic Vignesh M (CS20B045)
*/

#include "rob.h"

using namespace std;

int PC = 0;             //Program counter
int arithmetic_instructions = 0, logical_instructions = 0, data_instructions = 0, control_instructions = 0, halt_instructions = 0;  //Instruction counters
int no_of_cycles = 0, no_of_stalls = 0, no_of_instructions = 0;                                         //Cycle counters
int data_stalls = 0, control_stalls = 0;                                                                //Stall counters   
string inst_reg;                                                                                        //Instruction register

bool jump = false, halt = false;

ifstream i_rd, r_rd;                        //Input and output file readers
ofstream r_wr;
fstream dp;

int running = 0;                            //Flag to indicate if the processor is running

class fetch_buffer      //buffer between fetch and decode stages
{
    public:
    char inst[4];
    bool busy;
    fetch_buffer(): busy(false){}
}f_bf;

class decode_buffer     //buffer between decode and execute stages
{
    public:
    bool dependency, jump;
    int opcode;
    int a,b,c;
    decode_buffer()
    {
        dependency = false;
        jump = false;
        opcode = -1;
        a = 0;
        b = 0;
        c = 0;
    }
}d_bf;

class execute_buffer    //buffer between execute and memory stages
{
    public:
    int opcode;
    int val, add;
}e_bf;

class memory_buffer     //buffer between memory and writeback stages
{
    public:
    int opcode;
    int reg, val;
}m_bf;

class Register          //Register class to implement Architectural register file
{
    public:
    int val;
    bool free;
    Register()
    {
        val = 0;
        free = true;
    }
}reg[16];

class Forwarder         //Forwarding register file
{
    public:
    bool valid;
    int val;
    Forwarder()
    {
        valid = false;
        val = 0;
    }
}f[16];


class FU_bf        // Functional units' buffers
{
public:
    bool free;
    reservation_station bf;
    FU_bf(): free(true){}
}alu_bf, load_bf, str_bf, jmp_bf, br_bf;


void instruction_fetch()
{
    // cout<<jump<<" "<<halt<<endl;
    if(i_rd.eof() || jump || halt)  return;
    // cout<<"yes"<<endl;
    running++;
    f_bf.busy = true;

    i_rd.seekg(PC*3);                   //Reading from ICache
    getline(i_rd, inst_reg, '\n');
    f_bf.inst[0] = inst_reg[0], f_bf.inst[1] = inst_reg[1];
    getline(i_rd, inst_reg, '\n');
    f_bf.inst[2] = inst_reg[0], f_bf.inst[3] = inst_reg[1];
    
    PC = PC + 2;                    //Update PC
}

void instruction_decode()
{
    running++;
    int OPCode=0, dest=0, source1=0, source2=0;     //Split instruction into 4 parts
    OPCode  = (f_bf.inst[0]<='9')?(f_bf.inst[0]-'0'):(f_bf.inst[0]-'a'+10);
    dest    = (f_bf.inst[1]<='9')?(f_bf.inst[1]-'0'):(f_bf.inst[1]-'a'+10);
    source1 = (f_bf.inst[2]<='9')?(f_bf.inst[2]-'0'):(f_bf.inst[2]-'a'+10);
    source2 = (f_bf.inst[3]<='9')?(f_bf.inst[3]-'0'):(f_bf.inst[3]-'a'+10);
    f_bf.busy = false;
    if(OPCode == 10 || OPCode == 11) jump = true;
    if(OPCode == 15) 
    {
        halt = true;
        return;
    }
    int s[3] = {dest, source1, source2};    // 0, 1, 2, 4, 5, 7(alu, 2 operands), 6(not), 8(load), 9(store)
    if(OPCode == 3) s[2] = 1;               // second operand is 1 in INC function

    bool ret = dispatch_to_rsrob(OPCode, s);
}

void update_rob_entry(reservation_station bf, int value)
{
    running++;
    int rb = bf.rob_index;
    if(bf.opcode <= 8)     // ALU/Load op, have to update registers
    {
        int rf_ind = rob[rb].rename_tag;    // corresponding rename register
        rrf[rf_ind].data = value;
        rrf[rf_ind].valid = true, rrf[rf_ind].busy = false;

        // cout<<"tag is "<<rf_ind<<endl;
        for(int i = 0; i < REG_COUNT; i++)
        {
            if(arf[i].tag == rf_ind)        // Architectural register whose tag is rf_ind
            {
                // cout<<i<<" "<<arf[i].tag<<endl;
                arf[i].data = value;
                arf[i].busy = false;
                arf[i].tag = -1;
            }
        }

        for(int i = 0; i < RES_SIZE; i++)   // matching the tag with entries in reservation station
        {
            if(res[i].busy)
            {
                if(!res[i].valid[0] && res[i].ops[0] == rf_ind)
                {
                    res[i].ops[0] = value;
                    res[i].valid[0] = true;
                }
                if(!res[i].valid[1] && res[i].ops[1] == rf_ind)     // in case of load, valid[1] will always be true, as it's an immediate value
                {
                    res[i].ops[1] = value;
                    res[i].valid[1] = true;
                }
                if(res[i].valid[0] && res[i].valid[1]) res[i].ready = true;
            }
        }
    }
    rob[rb].finished = true;
    rob[rb].busy = false;
}

void ALU()         //Arithmetic Logic Unit
{
    running++;
    alu_bf.free = true;
    int opcode = alu_bf.bf.opcode;
    int a = alu_bf.bf.ops[0];
    int b = alu_bf.bf.ops[1];
    int value;
    switch(opcode)
    {
        case 0:         //ADD
        {
            value = a+b;
            break;
        }
        case 1:         //SUB
        {
            value = a-b;
            break;
        }
        case 2:         //MUL
        {
            value = a*b;
            break;
        }
        case 3:         //INC
        {
            value = a+1;
            break;
        }
        case 4:         //AND   
        {
            value = a&b;
            break;
        }
        case 5:         //OR
        {
            value = a|b;
            break;
        }
        case 6:         //NOT
        {
            value = ~a;
            break;
        }
        case 7:         //XOR
        {
            value = a^b;
            break;
        }
    }
    update_rob_entry(alu_bf.bf, value);
}

void loadFU()
{
    running++;
    load_bf.free = true;
    int base = load_bf.bf.ops[0], offset = load_bf.bf.ops[1];
    int mem_loc = base+offset;

    dp.seekg(mem_loc*3);       // read DCache
    string in;
    getline(dp, in, '\n');

    int value = 0;
    
    if(in[0] <= '9')    value += (in[0]-'0')<<4;
    else                value += (in[0]-'a'+10)<<4;
    
    if(in[1] <= '9')    value += in[1]-'0';
    else                value += in[1]-'a'+10;
    
    if(value & 128)  value -= 256;

    update_rob_entry(load_bf.bf, value);
}

void storeFU()
{
    running++;
    str_bf.free = true;
    int base = str_bf.bf.ops[0], offset = str_bf.bf.ops[1];
    int mem_loc = base+offset;

    int value = str_bf.bf.dest;

    dp.seekg(mem_loc*3);
    char low, high;
    
    if(value < 0)               value += (1<<8);
    
    if((value & 15) <= 9)       low = '0' + (value&15);
    else                        low = 'a' + (value&15)-10;
    
    value = value>>4;
    
    if((value&15) <= 9)         high = '0' + (value&15);
    else                        high = 'a' + (value&15)-10;
    
    dp.put(high);
    dp.put(low);

    update_rob_entry(str_bf.bf, value);
}

void jumpFU()
{
    running++;
    jmp_bf.free = true;
    int high = jmp_bf.bf.ops[0], low = jmp_bf.bf.ops[1];
    int L1 = ((high<<4) + low - ((high&8)?(1<<8):0));
    PC += L1;
    jump = false;    
    update_rob_entry(jmp_bf.bf, -1);
}

void branchFU()
{
    running++;
    br_bf.free = true;
    int high = br_bf.bf.ops[0], low = br_bf.bf.ops[1];
    int L1 = ((high<<4) + low - ((high&8)?(1<<8):0));
    if(br_bf.bf.dest == 0) PC += L1;
    jump = false;
    update_rob_entry(br_bf.bf, -1);
}

void dispatch_to_FU()
{
    for(int i=0; i<RES_SIZE; i++)
    {
        if(res[i].busy && res[i].ready)
        {
            int rb = res[i].rob_index;
            if(rob[rb].issued) continue;
            running++;
            if(res[i].opcode == 15)                 //if HALT instruction, return
                return;

            switch(res[i].opcode)
            {
                // All ALU instructions
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                {
                    if(alu_bf.free)
                    {
                        alu_bf.free = false;
                        alu_bf.bf = res[i];
                        res[i].busy = false;
                        rob[rb].issued = true;
                    } 
                    break;
                }
                case 8:         // load
                {
                    if(load_bf.free)
                    {
                        load_bf.free = false;
                        load_bf.bf = res[i];
                        res[i].busy = false;
                        rob[rb].issued = true;
                    }
                    break;
                }   
                case 9:         // store
                {
                    if(str_bf.free)
                    {
                        str_bf.free = false;
                        str_bf.bf = res[i];
                        res[i].busy = false;
                        rob[rb].issued = true;
                    }
                    break;
                }
                case 10:        // jump
                {
                    if(jmp_bf.free)
                    {
                        jmp_bf.free = false;
                        jmp_bf.bf = res[i];
                        res[i].busy = false;
                        rob[rb].issued = true;
                    }
                    break;
                }
                case 11:        // branch
                {
                    if(br_bf.free)
                    {
                        br_bf.free = false;
                        br_bf.bf = res[i];
                        res[i].busy = false;
                        rob[rb].issued = true;
                    }
                    break;
                }
            }
        }
    }
}

void open_files()
{
    i_rd.open("ICache.txt");
    if(!i_rd.is_open())
    {
        cout << "Error opening ICache.txt" << endl;
        exit(1);
    }

    dp.open("DCache.txt");
    if(!dp.is_open())
    {
        cout << "Error opening DCache.txt" << endl;
        exit(1);
    }
}

void read_RF()
{
    r_rd.open("RF.txt");
    for(int i = 0; i < REG_COUNT; i++)
    {
        string in;
        getline(r_rd, in, '\n');
        arf[i].data = 0;
        
        if(in[0] <= '9') arf[i].data += (in[0]-'0')<<4;
        else arf[i].data += (in[0]-'a'+10)<<4;
        
        if(in[1] <= '9') arf[i].data += in[1]-'0';
        else arf[i].data += in[1]-'a'+10;
        
        if(arf[i].data & 128) arf[i].data -= 256;
    }
}

void print_stats()
{
    ofstream fout("Output.txt");
    no_of_stalls = no_of_cycles - no_of_instructions - 4;
    fout << "Total number of instructions executed: " << no_of_instructions << endl;
    fout << "Number of instructions in each class " << endl;   
    fout << "Arithmetic instructions              : " << arithmetic_instructions << endl;
    fout << "Logical instructions                 : " << logical_instructions << endl;
    fout << "Data instructions                    : " << data_instructions << endl;
    fout << "Control instructions                 : " << control_instructions << endl;
    fout << "Halt instructions                    : " << halt_instructions << endl;
    fout << "Cycles Per Instruction               : " << (double)no_of_cycles/no_of_instructions << endl;
    fout << "Total number of stalls               : " << no_of_stalls << endl;
    fout << "Data stalls (RAW)                    : " << data_stalls << endl;
    fout << "Control stalls                       : " << control_stalls << endl;
    fout.close();
}

void make_outputs()
{   
    dp.close();
    ifstream d_copy("DCache.txt");
    ofstream d_write;
    d_write.open("ODCache.txt");
    while(!d_copy.eof())
    {
        string in;
        getline(d_copy, in, '\n');
        d_write << in << endl;
    }
    d_copy.close();
    d_write.close();

    print_stats();              //print statistics
}

void update_RF()
{
    ofstream reg_out;
    reg_out.open("RF.txt");
    for(int i = 0; i < 16; i++)
    {
        char low, high;

        if(arf[i].data < 0)    arf[i].data += (1<<8);
        
        if((arf[i].data & 15) <= 9)    low = '0' + (arf[i].data&15);
        else                        low = 'a' + (arf[i].data&15)-10;
        
        arf[i].data = arf[i].data>>4;
        
        if((arf[i].data&15) <= 9)      high = '0' + (arf[i].data&15);
        else                        high = 'a' + (arf[i].data&15)-10;
        
        reg_out << high;
        reg_out << low;
        reg_out << endl;
    }
    reg_out.close();
}

void close_files()
{
    i_rd.close();
    r_rd.close();
}

int main()
{
    open_files();
    read_RF();

    for(int i = 0; i < REG_COUNT; i++)
    {
        cout<<i+1<<" "<<arf[i].data<<endl;
    } 

    do
    {
        /* 
            Functional units:   (check buffers)
                branch
                jump
                store
                load
                alu
                if done, go match the destination's tag with rs entries, arf, rrf, pop out of rob
            Dispatch from rs
            Decode
            Fetch if jump == false
        */
        running = 0;
       if(!br_bf.free) branchFU();
       if(!jmp_bf.free) jumpFU();
       if(!str_bf.free) storeFU();
       if(!load_bf.free) loadFU();
       if(!alu_bf.free) ALU();

       dispatch_to_FU();
       if(f_bf.busy) instruction_decode();
       instruction_fetch();

        cout<<"up "<<running<<endl;
        if(running)     no_of_cycles++;

    }while(running);
    for(int i = 0; i < REG_COUNT; i++)
    {
        cout<<i+1<<" "<<arf[i].data<<endl;
    }

    update_RF();                    //Update RF
    make_outputs();                 //Ready ODCache and Output.txt
    close_files();                  //Close files
}
