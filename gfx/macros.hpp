#ifndef UUID_16A8D91C6BEA4174A45E11A5F85FB93C
#define UUID_16A8D91C6BEA4174A45E11A5F85FB93C

inline uint8_t choose(uint8_t this_, uint8_t if_this, uint8_t is_different_from_this, uint8_t otherwise_this)
{
	int32_t mask = (int32_t(if_this^is_different_from_this) - 1) >> 31;
	return this_ ^ ((this_^otherwise_this) & mask);
}

#if 0
#define SCALE2X() do { \
    if(B != H && F != D) { \
        R1 = D == B ? B : E; \
        R2 = B == F ? F : E; \
        R4 = F == H ? H : E; \
        R3 = H == D ? D : E; \
    } else { \
        R1 = E; \
        R2 = E; \
        R4 = E; \
        R3 = E; \
    } \
} while(0)
#else

#define SCALE2X_DECL uint32_t R1, R2, R3, R4

#define SCALE2X() do { \
	if(B != H && F != D) { \
        R1 = choose(E, D, B, B); \
        R2 = choose(E, B, F, F); \
        R4 = choose(E, F, H, H); \
        R3 = choose(E, H, D, D); \
    } else { \
        R1 = E; \
        R2 = E; \
        R4 = E; \
        R3 = E; \
    } \
} while(0)
#endif

#define WRITER_2X_8(dest) do { \
    uint8_t* pix_2x_dest_ = (dest); \
    pix_2x_dest_[0] = R1; \
    pix_2x_dest_[1] = R2; \
    pix_2x_dest_[downOffset] = R3; \
    pix_2x_dest_[downOffset+1] = R4; \
} while(0)

#define READER_8(x, src) do { \
	x = *(src); \
} while(0)

#define SHIFT_X() do { \
    A = B; \
    B = C; \
    D = E; \
    E = F; \
    G = H; \
    H = I; \
} while(0)

#define FILTER_X(dest, destPitch, src, srcPitch, width, height, srcSize, destSize, FUNC, ATTRIB, READER, WRITER) do { \
    uint8_t* dest_ = (dest); \
    uint8_t const* src_ = (src); \
    int destPitch_ = (destPitch); \
    int srcPitch_ = (srcPitch); \
    int width_ = (width); \
    int height_ = (height); \
    int srcSize_ = (srcSize); \
    int destSize_ = (destSize); \
    ATTRIB; \
    uint32_t A, B, C, D, E, F, G, H, I; \
    /* First line */ \
    { \
        uint8_t const* line_  = src_; \
        uint8_t const* lineB_ = src_ + srcPitch_; \
        uint8_t* destLine_  = dest_; \
        A = 0; B = 0            ; C = 0                        ; \
        D = 0; READER(E, line_) ; READER(F, line_  += srcSize_); \
        G = 0; READER(H, lineB_); READER(I, lineB_ += srcSize_); \
        /* First pixel on first line */ \
		for(int x_ = 0;;) { \
			FUNC(); \
			WRITER(destLine_); \
			SHIFT_X(); \
			destLine_ += destSize_; \
			if(++x_ >= width - 1) \
				break; \
			line_ += srcSize_; lineB_ += srcSize_; \
			READER(F, line_); \
			READER(I, lineB_); \
		} \
        C = 0; F = 0; I = 0; \
        /* Last pixel on first line */ \
        FUNC(); \
        WRITER(destLine_); \
    } \
    for(int y_ = 1; y_ < height - 1; ++y_) \
    { \
        uint8_t const* lineA_ = src_ + (y_-1)*srcPitch_; \
        uint8_t const* line_  = src_ + y_*srcPitch_; \
        uint8_t const* lineB_ = src_ + (y_+1)*srcPitch; \
        uint8_t* destLine_  = dest_ + y_*destPitch; \
        A = 0; READER(B, lineA_); READER(C, lineA_ += srcSize_); \
        D = 0; READER(E, line_) ; READER(F, line_  += srcSize_); \
        G = 0; READER(H, lineB_); READER(I, lineB_ += srcSize_); \
        /* First pixel */ \
		for(int x_ = 0;;) { \
			FUNC(); \
			WRITER(destLine_); \
			SHIFT_X(); \
			destLine_ += destSize_; \
			if(++x_ >= width - 1) \
				break; \
			lineA_ += srcSize_; line_ += srcSize_; lineB_ += srcSize_; \
			READER(C, lineA_); \
			READER(F, line_); \
			READER(I, lineB_); \
		} \
        /* Last pixel */ \
        C = 0; F = 0; I = 0; \
        FUNC(); \
        WRITER(destLine_); \
    } \
    /* Last line */ \
    { \
        uint8_t const* lineA_ = src_ + (height-2)*srcPitch_; \
        uint8_t const* line_  = src_ + (height-1)*srcPitch_; \
        uint8_t* destLine_  = dest_ + (height-1)*destPitch; \
        A = 0; READER(B, lineA_); READER(C, lineA_ += srcSize_); \
        D = 0; READER(E, line_) ; READER(F, line_  += srcSize_); \
        G = 0; H = 0            ; I = 0                        ; \
        /* First pixel on last line */ \
		for(int x_ = 0;;) { \
			FUNC(); \
			WRITER(destLine_); \
			SHIFT_X(); \
			destLine_ += destSize_; \
			if(++x_ >= width - 1) \
				break; \
			lineA_ += srcSize_; line_ += srcSize_; \
			READER(C, lineA_); \
			READER(F, line_); \
		} \
        /* Last pixel on last line */ \
        C = 0; F = 0; I = 0; \
        FUNC(); \
        WRITER(destLine_); \
    } \
} while(0)

#endif // UUID_16A8D91C6BEA4174A45E11A5F85FB93C
