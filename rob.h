#ifndef ROB_H
#define ROB_H

#include <bits/stdc++.h>
#include "rfuncs.h"

const int ROB_SIZE = 64;
const int RES_SIZE = 32;

class ROB
{
    public:
    bool busy;
    bool issued;
    bool finished;
    int opcode;
    int rename_tag;
    bool speculative;
    bool valid;

    ROB(): busy(false), issued(false), finished(false), rename_tag(-1), speculative(false), valid(false){}
};

class reservation_station
{
    public:
    bool busy, valid[2];
    bool ready;
    int opcode;
    int ops[2];
    int dest;
    int rob_index;

    reservation_station()
    {
        busy = false;
        valid[0] = valid[1] = false;
        ready = false;  
        rob_index = -1;
        ops[0] = ops[1] = -1;
    }
};

std::vector<ROB> rob(ROB_SIZE);
std::vector<reservation_station> res(RES_SIZE);

int next_complete=0, next_allocate=0;

int find_free_res()
{
    for(int i=0; i<RES_SIZE; i++)
    {
        if(!res[i].busy) return i;
    }
    return -1;
}

int find_free_rob()
{
    for(int i = 0; i < ROB_SIZE; i++)
    {
        if(!rob[i].busy) return i;
    }
    return -1;
}

bool dispatch_to_rsrob(int OPcode, int s[3])
{
    switch(OPcode)
    {  
        case 0:             //ADD instruction
        case 1:             //SUB instruction
        case 2:             //MUL instruction
        case 4:             //AND instruction
        case 5:             //OR instruction
        case 7:             //XOR instruction
        {
            reservation_station temp;
            int res_index = find_free_res();
            if(res_index == -1)
                return false;
            
            temp.busy = true;
            temp.opcode = OPcode;

            //dest  -->  s[0]
            int rrf_ind = get_free_rr();
            if(rrf_ind == -1) return false;     // no free rrf

            rrf[rrf_ind].busy = true;           // rrf_ind is free
            rrf[rrf_ind].valid = false;
            arf[s[0]].busy = true;
            arf[s[0]].tag = rrf_ind;

            temp.dest = rrf_ind;

            //source --> s[1]
            if(!arf[s[1]].busy)
            {
                temp.ops[0] = arf[s[1]].data;
                temp.valid[0] = true;
            }
            else
            {
                if(rrf[arf[s[1]].tag].valid)
                {
                    temp.ops[0] = rrf[arf[s[1]].tag].data;
                    temp.valid[0] = true;
                }
                else
                {
                    temp.ops[0] = arf[s[1]].tag;
                    temp.valid[0] = false;
                }
            }

            //source --> s[2]
            if(!arf[s[2]].busy)
            {
                temp.ops[1] = arf[s[2]].data;
                temp.valid[1] = true;
            }
            else
            {
                if(rrf[arf[s[2]].tag].valid)
                {
                    temp.ops[1] = rrf[arf[s[2]].tag].data;
                    temp.valid[1] = true;
                }
                else
                {
                    temp.ops[1] = arf[s[2]].tag;
                    temp.valid[1] = false;
                }
            }

            if(temp.valid[1] && temp.valid[0]) temp.ready = true;
            int free_rb = find_free_rob();
            if(free_rb == -1) return false;

            temp.rob_index = free_rb;
            res[res_index] = temp;
            
            rob[free_rb].busy = true;
            rob[free_rb].issued = false;
            rob[free_rb].finished = false;
            rob[free_rb].opcode = OPcode;
            rob[free_rb].rename_tag = rrf_ind;
            rob[free_rb].speculative = false;
            rob[free_rb].valid = true;

            std::cout<<"res "<<temp.ops[0]<<" "<<temp.ops[1]<<" "<<temp.valid[0]<<" "<<temp.valid[1]<<" "<<temp.opcode<<" "<<rob[free_rb].rename_tag<<std::endl;

            return true;
        }
        
        case 3:             //INC instruction
        case 6:             //NOT instruction
        case 8:             //LOAD instruction
        {
            reservation_station temp;
            int res_index = find_free_res();
            if(res_index == -1)
                return false;
            
            temp.busy = true;
            temp.opcode = OPcode;

            //dest  -->  s[0]
            int rrf_ind = get_free_rr();
            if(rrf_ind == -1) return false;     // no free rrf

            rrf[rrf_ind].busy = true;           // rrf_ind is free
            rrf[rrf_ind].valid = false;
            arf[s[0]].busy = true;
            arf[s[0]].tag = rrf_ind;

            temp.dest = rrf_ind;

            // base addr --> s[1]
            if(!arf[s[1]].busy)
            {
                temp.ops[0] = arf[s[1]].data;
                temp.valid[0] = true;
            }
            else
            {
                if(rrf[arf[s[1]].tag].valid)
                {
                    temp.ops[0] = rrf[arf[s[1]].tag].data;
                    temp.valid[0] = true;
                }
                else
                {
                    temp.ops[0] = arf[s[1]].tag;
                    temp.valid[0] = false;
                }
            }
            temp.ops[1] = s[2];     // 1 for INC, OFFSET for LOAD
            temp.valid[1] = true;   // no need to fetch from a register
            if(temp.valid[1] && temp.valid[0]) temp.ready = true;
            int free_rb = find_free_rob();
            if(free_rb == -1) return false;

            temp.rob_index = free_rb;
            res[res_index] = temp;
            
            rob[free_rb].busy = true;
            rob[free_rb].issued = false;
            rob[free_rb].finished = false;
            rob[free_rb].opcode = OPcode;
            rob[free_rb].rename_tag = rrf_ind;
            rob[free_rb].speculative = false;
            rob[free_rb].valid = true;
            return true;
        }

        case 9:         //STORE instruction     STORE R1, R2, X ; [R2+X] = R1
        {
            int res_index = find_free_res();
            if(res_index == -1)
                return false;
            
            reservation_station temp;
            temp.busy = true;
            temp.opcode = OPcode;

            //value to be stored --> s[0],  corresponds to valid[0]
            if(!arf[s[0]].busy)
            {
                temp.dest = arf[s[0]].data;
                temp.valid[0] = true;
            }
            else
            {
                if(rrf[arf[s[0]].tag].valid)
                {
                    temp.dest = rrf[arf[s[0]].tag].data;
                    temp.valid[0] = true;
                }
                else
                {
                    temp.dest = arf[s[0]].tag;
                    temp.valid[0] = false;
                }
            }

            //base address --> s[1], corresponds to valid[1]
            if(!arf[s[1]].busy)
            {
                temp.ops[0] = arf[s[1]].data;
                temp.valid[1] = true;
            }
            else
            {
                if(rrf[arf[s[1]].tag].valid)
                {
                    temp.ops[0] = rrf[arf[s[1]].tag].data;
                    temp.valid[1] = true;
                }
                else
                {
                    temp.ops[0] = arf[s[1]].tag;
                    temp.valid[1] = false;
                }
            }
            temp.ops[1] = s[2];     // OFFSET for STORE
            temp.valid[1] = true;
            if(temp.valid[1] && temp.valid[0]) temp.ready = true;
            int free_rb = find_free_rob();
            if(free_rb == -1) return false;

            temp.rob_index = free_rb;
            res[res_index] = temp;
            
            rob[free_rb].busy = true;
            rob[free_rb].issued = false;
            rob[free_rb].finished = false;
            rob[free_rb].opcode = OPcode;
            rob[free_rb].rename_tag = -1;
            rob[free_rb].speculative = false;
            rob[free_rb].valid = true;
            return true;
        }
        case 10:        //JMP instruction   JMP L1 ; PC += L1,   L1 --> dest and s[0], alu has to compute L1[7:4] + 16*L1[3:0]
        {
            int res_index = find_free_res();
            if(res_index == -1)
                return false;

            reservation_station temp;
            temp.busy = true;
            temp.opcode = OPcode;

            //L1 most significant 4 bits  -->  dest
            temp.ops[0] = s[0];
             //L1 least significant 4 bits  -->  source1
            temp.ops[1] = s[1];
            temp.valid[0] = temp.valid[1] = temp.ready = true;

            int free_rb = find_free_rob();
            if(free_rb == -1) return false;

            temp.rob_index = free_rb;
            res[res_index] = temp;
            
            rob[free_rb].busy = true;
            rob[free_rb].issued = false;
            rob[free_rb].finished = false;
            rob[free_rb].opcode = OPcode;
            rob[free_rb].rename_tag = -1;
            rob[free_rb].speculative = false;
            rob[free_rb].valid = true;

            return true;
        }

        case 11:        //BEQZ instruction, BEQZ R1, L1 ;Jump to L1 if R1 content is zero.
        {
            int res_index = find_free_res();
            if(res_index == -1)
                return false;

            reservation_station temp;
            temp.busy = true;
            temp.opcode = OPcode;

            // Reading from R1 register, putting it in 'dest' variable
            if(!arf[s[0]].busy)
            {
                temp.dest = arf[s[0]].data;
                temp.valid[0] = true;
            }
            else
            {
                if(rrf[arf[s[0]].tag].valid)
                {
                    temp.dest = rrf[arf[s[0]].tag].data;
                    temp.valid[0] = true;
                }
                else
                {
                    temp.dest = arf[s[0]].tag;
                    temp.valid[0] = false;
                }
            }

            //L1 most significant 4 bits  -->  source1
            temp.ops[0] = s[1];
             //L1 least significant 4 bits  -->  source2
            temp.ops[1] = s[2];
            temp.valid[1] = true;

            if(temp.valid[0] && temp.valid[1]) temp.ready = true;

            int free_rb = find_free_rob();
            if(free_rb == -1) return false;

            temp.rob_index = free_rb;
            res[res_index] = temp;
            
            rob[free_rb].busy = true;
            rob[free_rb].issued = false;
            rob[free_rb].finished = false;
            rob[free_rb].opcode = OPcode;
            rob[free_rb].rename_tag = -1;
            rob[free_rb].speculative = false;
            rob[free_rb].valid = true;

            return true;
        }

        case 15:
        {
            
            break;
        }

    }
}

#endif