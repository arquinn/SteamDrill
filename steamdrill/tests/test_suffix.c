#include <string.h>
#include <stdio.h>

int check_suffix(const char* full, const char*suffix)
{
  size_t lensuffix = strlen(suffix), lenfull = strlen(full);
   return lenfull < lensuffix || strcmp(suffix, full + lenfull - lensuffix);
}

int main(){
  static const char needle[] ="libdl.so";
  static const char blah[] = "long_path_with_lots";
  static const char blah2[] = "tinie";
  static const char long_match[] = "long_withc_match_libdl.so";

  printf("check %d\n", check_suffix(blah2, needle));
  printf("check %d\n", check_suffix(blah, needle));
  printf("check %d\n", check_suffix(long_match, needle));

  
}
