static volatile bool b;

void f(){
  int* p; int a; 
  if(b) a = 2; else p = &a;
}

int main(){
  int c;
  f();
  return 0;
}
