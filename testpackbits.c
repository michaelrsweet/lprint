//
// PackBits unit test program
//
// Usage:
//
//   ./testpackbits [COUNT]
//
// Copyright © 2024 by Michael R Sweet
//

#include "lprint.h"
#include "test.h"


//
// Local functions...
//

static unsigned get_rand(void);


//
// 'main()' - Main entry for test program.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i,			// Looping var
		num_tests;		// Number of tests to run
  size_t	offset,			// Offset in source buffer
		count,			// Looping var
		len;			// Length of sequence
  unsigned char	src[1024];		// Source buffer
  unsigned char	*dst;			// Destination buffer
  size_t	dstlen,			// Number of destination bytes
		dstmax;			// Maximum number of destination bytes


  // See if the number of tests is on the command-line...
  if (argc == 1)
  {
    num_tests = 1000000;
  }
  else if (argc > 2 || (num_tests = atoi(argv[1])) < 1)
  {
    fputs("Usage: ./testpackbits [COUNT]\n", stderr);
    return (1);
  }

  // Maximum expansion as defined by the PackBits algorithm...
  dstmax = sizeof(src) + sizeof(src) / 128;

  // Allocate PackBits buffer
  testBegin("lprintPackBitsAlloc(%u)", (unsigned)(2 * sizeof(src)));
  if ((dst = lprintPackBitsAlloc(2 * sizeof(src))) != NULL)
    testEnd(true);
  else
    testEndMessage(false, "%s", strerror(errno));

  // Loop many times with random changes to the source buffer to test different
  // source values...
  memset(src, 0, sizeof(src));

  testBegin("lprintPackBitsCompress(%d times)", num_tests);

  for (i = 0; i < num_tests; i ++)
  {
    // Show progress...
    testProgress();

    // Make a random change to the source buffer...
    len    = (get_rand() % (sizeof(src) / 4)) + 1;
    offset = get_rand() % (sizeof(src) - len);

    if (get_rand() & 1)
    {
      // Add random bytes
      for (count = 0; count < len; count ++)
        src[offset + count] = get_rand();
    }
    else
    {
      // Add constant bytes
      memset(src + offset, get_rand(), len);
    }

    // Compress the result
    if ((dstlen = lprintPackBitsCompress(dst, src, sizeof(src))) == 0 || dstlen > dstmax)
    {
      testEndMessage(false, "%u > %u", (unsigned)dstlen, (unsigned)dstmax);
      testError("\nSource Buffer:");
      testHexDump(src, sizeof(src));
      testError("\nDestination Buffer:");
      testHexDump(dst, dstlen);
      break;
    }
  }

  if (i >= num_tests)
    testEnd(true);

  return (testsPassed ? 0 : 1);
}


//
// 'get_rand()' - Return a 32-bit pseudo-random number.
//
// This function returns a 32-bit pseudo-random number suitable for use as
// one-time identifiers or nonces.  The random numbers are generated/seeded
// using system entropy.
//

static unsigned				// O - Random number
get_rand(void)
{
#if _WIN32
  // rand_s uses real entropy...
  unsigned v;				// Random number


  rand_s(&v);

  return (v);

#elif defined(__APPLE__)
  // macOS/iOS arc4random() uses real entropy automatically...
  return (arc4random());

#else
  // Use a Mersenne twister random number generator seeded from /dev/urandom...
  unsigned	i,			// Looping var
		temp;			// Temporary value
  static bool	first_time = true;	// First time we ran?
  static unsigned mt_state[624],	// Mersenne twister state
		mt_index;		// Mersenne twister index


  if (first_time)
  {
    int		fd;			// "/dev/urandom" file
    struct timeval curtime;		// Current time

    // Seed the random number state...
    if ((fd = open("/dev/urandom", O_RDONLY)) >= 0)
    {
      // Read random entropy from the system...
      if (read(fd, mt_state, sizeof(mt_state[0])) < sizeof(mt_state[0]))
        mt_state[0] = 0;		// Force fallback...

      close(fd);
    }
    else
      mt_state[0] = 0;

    if (!mt_state[0])
    {
      // Fallback to using the current time in microseconds...
      gettimeofday(&curtime, NULL);
      mt_state[0] = (unsigned)(curtime.tv_sec + curtime.tv_usec);
    }

    mt_index = 0;

    for (i = 1; i < 624; i ++)
      mt_state[i] = (unsigned)((1812433253 * (mt_state[i - 1] ^ (mt_state[i - 1] >> 30))) + i);

    first_time = false;
  }

  if (mt_index == 0)
  {
    // Generate a sequence of random numbers...
    unsigned i1 = 1, i397 = 397;	// Looping vars

    for (i = 0; i < 624; i ++)
    {
      temp        = (mt_state[i] & 0x80000000) + (mt_state[i1] & 0x7fffffff);
      mt_state[i] = mt_state[i397] ^ (temp >> 1);

      if (temp & 1)
	mt_state[i] ^= 2567483615u;

      i1 ++;
      i397 ++;

      if (i1 == 624)
	i1 = 0;

      if (i397 == 624)
	i397 = 0;
    }
  }

  // Pull 32-bits of random data...
  temp = mt_state[mt_index ++];
  temp ^= temp >> 11;
  temp ^= (temp << 7) & 2636928640u;
  temp ^= (temp << 15) & 4022730752u;
  temp ^= temp >> 18;

  if (mt_index == 624)
    mt_index = 0;

  return (temp);
#endif // _WIN32
}
