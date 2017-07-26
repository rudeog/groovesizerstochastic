#include "protos.h"
// x-platform

// from pcg
struct pcg_state_setseq_8 {
   uint8_t state;
   uint8_t inc;
};

typedef struct pcg_state_setseq_8       pcg8i_random_t;
#define pcg8i_srandom_r                 pcg_setseq_8_srandom_r
#define PCG_DEFAULT_MULTIPLIER_8        141U
#define pcg8i_random_r                  pcg_setseq_8_rxs_m_xs_8_random_r
#define pcg8i_boundedrand_r             pcg_setseq_8_rxs_m_xs_8_boundedrand_r

inline uint8_t pcg_output_rxs_m_xs_8_8(uint8_t state)
{
   uint8_t word = ((state >> ((state >> 6u) + 2u)) ^ state) * 217u;
   return (word >> 6u) ^ word;
}
inline void pcg_setseq_8_step_r(struct pcg_state_setseq_8* rng)
{
   rng->state = rng->state * PCG_DEFAULT_MULTIPLIER_8 + rng->inc;
}

inline uint8_t pcg_setseq_8_rxs_m_xs_8_random_r(struct pcg_state_setseq_8* rng)
{
   uint8_t oldstate = rng->state;
   pcg_setseq_8_step_r(rng);
   return pcg_output_rxs_m_xs_8_8(oldstate);
}

// seed random seq
inline void pcg_setseq_8_srandom_r(struct pcg_state_setseq_8* rng,
   uint8_t initstate, uint8_t initseq)
{
   rng->state = 0U;
   rng->inc = (initseq << 1u) | 1u;
   pcg_setseq_8_step_r(rng);
   rng->state += initstate;
   pcg_setseq_8_step_r(rng);
}

inline uint8_t
pcg_setseq_8_rxs_m_xs_8_boundedrand_r(struct pcg_state_setseq_8* rng,
   uint8_t bound)
{
   uint8_t threshold = ((uint8_t)(-bound)) % bound;
   for (;;) {
      uint8_t r = pcg_setseq_8_rxs_m_xs_8_random_r(rng);
      if (r >= threshold)
         return r % bound;
      LOGMESSAGE(1,"Random thoughts...");
   }
}

static pcg8i_random_t rng;

// return 0..max-1
uint8_t 
rndRandom(uint8_t max)
{   
   return pcg8i_boundedrand_r(&rng, max);
}

void
rndSRandom(uint8_t initstate, uint8_t initseq)
{
   pcg8i_srandom_r(&rng, initstate, initseq);
}
