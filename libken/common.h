#ifndef COMMON_H
#define COMMON_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 255

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define log2(a) (log(a) / log(2))

#ifndef _WIN32
#define O_BINARY O_RDONLY
#endif

union INTEGER_LARGE
{
  struct
  {
    unsigned int LowPart;
    int HighPart;
  };
  struct
  {
    unsigned int LowPart;
    int HighPart;
  } u;
  long long QuadPart;
};

static inline int mul_div(int number, int numerator, int denominator)
{
  long long ret = number;
  ret *= numerator;
  ret /= denominator;
  return (int)ret;
}

#endif
