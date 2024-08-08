#include "inflate_impl.h"
#include "bits.h"

static int tl_reverse_bits(int v, int bits)
{
	assert(bits <= 16);
	// to bit reverse n bits, reverse 16 and shift
	// e.g. 11 bits, bit reverse and shift away 5
	return tl_reverse_bits16(v) >> (16-bits);
}

#define CHECK(f)  do { int r_ = (f); if (r_) return r_; } while(0)

static int zbuild_huffman(zhuffman *z, uint8 *sizelist, int num)
{
	int i,k=0;
	int code, next_code[16], sizes[17];

	// DEFLATE spec for generating codes
	memset(sizes, 0, sizeof(sizes));
	memset(z->fast, 255, sizeof(z->fast));
	for (i=0; i < num; ++i)
		++sizes[sizelist[i]];
	sizes[0] = 0;
	for (i=1; i < 16; ++i)
		assert(sizes[i] <= (1 << i));
	code = 0;
	for (i=1; i < 16; ++i) {
		next_code[i] = code;
		z->firstcode[i] = (uint16) code;
		z->firstsymbol[i] = (uint16) k;
		code = (code + sizes[i]);
		if (sizes[i])
			if (code-1 >= (1 << i)) return ZERR_BAD_CODELENGTHS;
		z->maxcode[i] = code << (16-i); // preshift for inner loop
		code <<= 1;
		k += sizes[i];
	}
	z->maxcode[16] = 0x10000; // sentinel
	for (i=0; i < num; ++i) {
		int s = sizelist[i];
		if (s) {
			int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
			z->size[c] = (uint8)s;
			z->value[c] = (uint16)i;
			if (s <= ZFAST_BITS) {
				int k = tl_reverse_bits(next_code[s],s);
				while (k < (1 << ZFAST_BITS)) {
					z->fast[k] = (uint16) c;
					k += (1 << s);
				}
			}
			++next_code[s];
		}
	}
	return ZERR_OK;
}

static int fill_bits(tl_inflate_source* z)
{
	while (z->num_bits <= 24)
	{
		assert(z->code_buffer < (1U << z->num_bits));

		// fill_bits does lookahead, so we need flush logic

		if (tl_bs_check(&z->base.in))
			z->code_buffer |= tl_bs_unsafe_get(&z->base.in) << z->num_bits;
		else if(!z->base.flush)
			return 1;
		// TODO: If z->base.flush, we should limit the number of zero bytes that are read, to avoid infinite looping

		z->num_bits += 8;
	}

	return 0;
}

static int zreceive(tl_inflate_source* z, int n, unsigned int* dest)
{
	if (z->num_bits < n)
		CHECK(fill_bits(z));
	*dest = z->code_buffer & ((1 << n) - 1);
	z->code_buffer >>= n;
	z->num_bits -= n;
	return 0;
}

static int zhuffman_decode(tl_inflate_source* a, zhuffman* z, unsigned int* dest)
{
	int b,s,k;
	if (a->num_bits < 16)
		CHECK(fill_bits(a));
	b = z->fast[a->code_buffer & ZFAST_MASK];
	if (b < 0xffff) {
		s = z->size[b];
		a->code_buffer >>= s;
		a->num_bits -= s;
		*dest = z->value[b];
		return 0;
	}

	// not resolved by fast table, so compute it the slow way
	// use jpeg approach, which requires MSbits at top
	k = tl_reverse_bits(a->code_buffer, 16);
	for (s = ZFAST_BITS+1; ; ++s)
		if (k < z->maxcode[s])
			break;
	if (s == 16) return ZERR_BAD_HUFFMAN_CODE; // invalid code!
	// code size is s, so:
	b = (k >> (16-s)) - z->firstcode[s] + z->firstsymbol[s];
	assert(z->size[b] == s);
	a->code_buffer >>= s;
	a->num_bits -= s;
	*dest = z->value[b];
	return 0;
}

static int length_base[31] = {
   3,4,5,6,7,8,9,10,11,13,
   15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258,0,0 };

static int length_extra[31]=
{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };

static int dist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};

static int dist_extra[32] =
{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,0,0};

static uint8 length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

// TODO: should statically initialize these for optimal thread safety
static uint8 default_length[288], default_distance[32];
static void init_defaults(void)
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     default_length[i]   = 8;
   for (   ; i <= 255; ++i)     default_length[i]   = 9;
   for (   ; i <= 279; ++i)     default_length[i]   = 7;
   for (   ; i <= 287; ++i)     default_length[i]   = 8;

   for (i=0; i <=  31; ++i)     default_distance[i] = 5;
}

static int parse_zlib_header(tl_inflate_source* a)
{
	unsigned int cmf, flg, cm, cinfo;
	CHECK(fill_bits(a));

	zreceive(a, 8, &cmf);
	zreceive(a, 8, &flg);
	cm    = cmf & 15;
	cinfo = cmf >> 4;


	if ((cmf*256+flg) % 31 != 0) return ZERR_BAD_HEADER; // zlib spec
	if (flg & 32) return ZERR_PRESET_DICT_NOT_SUPPORTED; // preset dictionary not allowed in png
	if (cm != 8) return ZERR_UNSUPPORTED_ENCODING; // DEFLATE required for png
	// window = 1 << (8 + cinfo)... but who cares, we fully buffer output
	return ZERR_OK;
}

static int parse_zlib(tl_inflate_source* self, int parse_header)
{
	BEGIN_YIELDABLE();
		self->num_bits = 0;
		self->code_buffer = 0;
		self->window_pos = 0;
		if (parse_header)
			YIELD_WE(19, parse_zlib_header(self));
		do {
			YIELD_WE(1, fill_bits(self));
			zreceive(self, 1, &self->final);
			zreceive(self, 2, &self->type);
			if (self->type == 0) {
				// Parse uncompressed block
				int nlen;
				unsigned int dummy;
				zreceive(self, self->num_bits & 7, &dummy); // discard
				// drain the bit-packed data into header
				self->i = 0;
				while (self->num_bits > 0) {
					self->header[self->i++] = (uint8) (self->code_buffer & 255); // wtf this warns?
					self->code_buffer >>= 8;
					self->num_bits -= 8;
				}
				assert(self->num_bits == 0);
				// now fill header the normal way
				while (self->i < 4) {
					YIELD_WE(3, tl_bs_check(&self->base.in) ? 0 : ZERR_UNDERFLOW);
					self->header[self->i++] = tl_bs_unsafe_get(&self->base.in);
				}
				self->len = self->header[1] * 256 + self->header[0];
				nlen = self->header[3] * 256 + self->header[2];
				if (nlen != (self->len ^ 0xffff)) return ZERR_STREAM_CORRUPT;

				while (self->len--) {
					uint8 b;
					YIELD_WE(4, (tl_bs_check(&self->base.in) ? (tl_bs_check_sink(&self->base.out) ? 0 : ZERR_OVERFLOW) : ZERR_UNDERFLOW));
					b = tl_bs_unsafe_get(&self->base.in);
					self->window[self->window_pos] = b;
					tl_bs_unsafe_put(&self->base.out, b);
					self->window_pos = (self->window_pos + 1) & 0x7fff;
				}
			} else if (self->type == 3) {
				return 0;
			} else {
				if (self->type == 1) {
					// use fixed code lengths
					if (!default_distance[0]) init_defaults();
					CHECK(zbuild_huffman(&self->z_length  , default_length  , 288));
					CHECK(zbuild_huffman(&self->z_distance, default_distance,  32));
				} else {
					// Compute huffman codes

					YIELD_WE(5, fill_bits(self));
					zreceive(self, 5, &self->hlit);
					zreceive(self, 5, &self->hdist);
					zreceive(self, 4, &self->hclen);

					self->hlit += 257;
					self->hdist += 1;
					self->hclen += 4;

					memset(self->codelength_sizes, 0, sizeof(self->codelength_sizes));
					for (self->i = 0; self->i < self->hclen; ++self->i) {
						unsigned int s;
						YIELD_WE(8, zreceive(self, 3, &s));
						self->codelength_sizes[length_dezigzag[self->i]] = (uint8) s;
					}
					CHECK(zbuild_huffman(&self->z_codelength, self->codelength_sizes, 19));

					self->i = 0;
					while (self->i < self->hlit + self->hdist) {
						unsigned int c;
						YIELD_WE(9, zhuffman_decode(self, &self->z_codelength, &c));
						assert(c >= 0 && c < 19);
						if (c < 16)
							self->lencodes[self->i++] = (uint8) c;
						else if (c == 16) {
							YIELD_WE(10, zreceive(self,2,&c));
							c += 3;
							memset(self->lencodes + self->i, self->lencodes[self->i - 1], c);
							self->i += c;
						} else if (c == 17) {
							YIELD_WE(11, zreceive(self,3,&c));
							c += 3;
							memset(self->lencodes + self->i, 0, c);
							self->i += c;
						} else {
							assert(c == 18);
							YIELD_WE(12, zreceive(self,7,&c));
							c += 11;
							memset(self->lencodes + self->i, 0, c);
							self->i += c;
						}
					}
					if (self->i != self->hlit + self->hdist) return ZERR_BAD_CODELENGTHS;
					CHECK(zbuild_huffman(&self->z_length, self->lencodes, self->hlit));
					CHECK(zbuild_huffman(&self->z_distance, self->lencodes + self->hlit, self->hdist));
				}

				// Parse huffman block

				for(;;) {
					unsigned int z;
					YIELD_WE(13, zhuffman_decode(self, &self->z_length, &z));
					if (z < 256) {
						self->z = z;
						YIELD_WE(14, tl_bs_check_sink(&self->base.out) ? 0 : ZERR_OVERFLOW);
						self->window[self->window_pos] = (uint8) self->z;
						tl_bs_unsafe_put(&self->base.out, (uint8) self->z);
						self->window_pos = (self->window_pos + 1) & 0x7fff;
					} else {
						unsigned int extra;
						if (z == 256) break;
						z -= 257;
						self->len = length_base[z];
						if (length_extra[z]) {
							self->z = z;
							YIELD_WE(15, zreceive(self, length_extra[self->z], &extra));
							self->len += extra;
						}

						YIELD_WE(16, zhuffman_decode(self, &self->z_distance, &z));
						if (z < 0) return ZERR_BAD_HUFFMAN_CODE;
						self->dist = dist_base[z];

						self->z = z;
						YIELD_WE(17, zreceive(self, dist_extra[self->z], &extra));
						self->dist += extra;

						while (self->len--) {
							uint8 b;
							YIELD_WE(18, tl_bs_check_sink(&self->base.out) ? 0 : ZERR_OVERFLOW);
							b = self->window[(self->window_pos - self->dist) & 0x7fff];
							self->window[self->window_pos] = b;
							tl_bs_unsafe_put(&self->base.out, b);
							self->window_pos = (self->window_pos + 1) & 0x7fff;
						}
					}
				}
			}
		} while (!self->final);

	END_YIELDABLE();
	return 0;
}

void tl_inf_init_(tl_inflate_source* self)
{
	YIELD_INIT(self);
	self->base.flush = 0;
}

tl_inflate* tl_inf_create()
{
	tl_inflate_source* self = malloc(sizeof(tl_inflate_source));
	tl_inf_init_(self);
	return &self->base;
}

int tl_inf_run(tl_inflate* self)
{
	return parse_zlib(((tl_inflate_source*)self), 1);
}

void tl_inf_destroy(tl_inflate* self)
{
	free(self);
}
