#ifndef FIXPOINT_H
#define FIXPOINT_H
#define fBase ((1<<14))
int Fixn(int n);
int Backn(int x);
int Addxy(int x,int y);
int Subxy(int x,int y);
int Addxn(int x,int n);
int Mulxy(int x,int y);
int Mulxn(int x,int n);
int Divxy(int x,int y);
int Divxn(int x,int n);
void PrintFix(int x);
#endif // FIXPOINT_H
