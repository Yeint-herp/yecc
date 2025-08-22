#ifndef UTILS_H
#define UTILS_H

static inline void set_bit_u32(unsigned *mask, unsigned bit, bool on) {
	if (on)
		*mask |= (1u << bit);
	else
		*mask &= ~(1u << bit);
}

static inline void set_bit_u64(unsigned long long *mask, unsigned bit, bool on) {
	if (on)
		*mask |= (1ull << bit);
	else
		*mask &= ~(1ull << bit);
}

#endif /* UTILS_H */
