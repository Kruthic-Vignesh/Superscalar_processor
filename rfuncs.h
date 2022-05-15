#ifndef RFUNC_H
#define RFUNC_H

#include "rf.h"

int get_free_rr()
{
	for(int i = 0; i < REG_COUNT; i++)
	{
		if(!rrf[i].busy) return i;
	}
	return -1;
}

#endif