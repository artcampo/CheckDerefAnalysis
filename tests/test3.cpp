int main(){
  int c;
  int* p;
  p = &c;
  c = 4;
  c = 4 + *p + 2;
  return 0;
}
