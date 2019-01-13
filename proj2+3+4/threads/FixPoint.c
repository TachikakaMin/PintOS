#ifndef FIXPOINT_C_INCLUDED
#define FIXPOINT_C_INCLUDED
#include "FixPoint.h"
#include <stdio.h>
/* Here we use functions.May be use macros better
    edit by wyg */
int Fixn(int n)                     //convert int to fixpoint
{
	return n*fBase;
}
int Backn(int x)                    //convert fixpoint back to int
{
	if(x>=0)
	   return (x+fBase/2)/fBase;
	else
	   return (x-fBase/2)/fBase;
}
int Addxy(int x,int y)              //fixpoint + fixpoint
{
	return x+y;
}
int Subxy(int x,int y)              //fixpoint - fixpoint
{
	return x-y;
}
int Addxn(int x,int n)              //fixpoint + int
{
	return x+n*fBase;
}
int Mulxy(int x,int y)              //fixpoint * fixpoint
{
	return ((int64_t)x)*y/fBase;
}
int Mulxn(int x,int n)              //fixpoint * int
{
	return x*n;
}
int Divxy(int x,int y)              //fixpoint / fixpoint
{
    if(y==0)
       {
           printf("div 0\n");
           return 0;
       }
	return ((int64_t)x)*fBase/y;
 //  return x/y;
}
int Divxn(int x,int n)               //fixpoint / int
{
	return x/n;
}
void PrintFix(int x)                 // print fixpoint number, 2 decemal
{
	printf("%d.%d",x>>14,Backn(Mulxn(x,100))%100);
}

#endif // FIXPOINT_C_INCLUDED
