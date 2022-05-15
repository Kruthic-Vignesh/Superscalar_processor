#ifndef RF_H
#define RF_H

const int REG_COUNT = 16;

class ARF
{
    public:
    int data;
    bool busy; 
    int tag;
    ARF(): tag(-1), busy(false), data(-1){}
}arf[REG_COUNT];

class RRF
{
    public:
    int data;
    bool valid, busy;
    RRF(): valid(false), busy(false), data(-1){}
}rrf[REG_COUNT];

#endif