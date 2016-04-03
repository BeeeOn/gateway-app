// Only for std::sto* and related

#ifdef CHECK_FUNCTION_EXISTS

#include <string>

#ifdef __CLASSIC_C__
int main(){
  int ac;
  char*av[];
#else
int main(int ac, char*av[]){
#endif
  std::string arg = "0";
  CHECK_FUNCTION_EXISTS(arg);
  if(ac > 1000)
    {
    return *av[0];
    }
  return 0;
}

#else  /* CHECK_FUNCTION_EXISTS */

#error "CHECK_FUNCTION_EXISTS has to specify the function"

#endif /* CHECK_FUNCTION_EXISTS */
