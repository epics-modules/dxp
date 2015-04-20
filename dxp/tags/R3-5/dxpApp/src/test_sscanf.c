#include <stdio.h>
#include <string.h>

#define NUM_STRINGS 4
#define NUM_FORMATS 4

int main(int argc, char *argv[])
{
  static const char *testStrings[NUM_STRINGS] = {"123", " 123", "123 ", " 123 "};
  static const char *testFormats[NUM_FORMATS] = {" %u %n", " %u%n", "%u %n", "%u%n"};
  unsigned ui;
  int i, j, nchars;
  
  for (i=0; i<NUM_STRINGS; i++) {
    for (j=0; j<NUM_FORMATS; j++) {
      ui = 0;
      nchars = 0;
      sscanf(testStrings[i], testFormats[j], &ui, &nchars);
      printf("String='%s' format='%s': ui=%u, strlen(string)=%d, nchars=%d\n", 
        testStrings[i], testFormats[j], ui, (int)strlen(testStrings[i]), nchars);
    }
  }
  return 0;  
}
