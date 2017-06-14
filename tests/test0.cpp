static volatile bool b;
int main(){
  int c, d; 
  if(b) c = 1; else d = 2;
  return 0;
}
