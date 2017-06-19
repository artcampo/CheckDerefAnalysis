int main(){
  int c;
  int* p;
  p = &c;
  *p = 1;
  c = 2;
  c = 3 + *p + 4;
  return 0;
}
