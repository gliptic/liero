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

#define PIX_X(dest, scale) do { \
    uint8_t* pix_2x_dest_ = (dest); \
    int pix_2x_scale_ = (scale); \
    if(pix_2x_scale_ == 2) { \
        pix_2x_dest_[0] = R1; \
        pix_2x_dest_[1] = R2; \
        pix_2x_dest_[destPitch_] = R3; \
        pix_2x_dest_[destPitch_+1] = R4; \
    } \
} while(0)

#define SHIFT_X() do { \
    A = B; \
    B = C; \
    D = E; \
    E = F; \
    G = H; \
    H = I; \
} while(0)

#define FILTER_X(dest, destPitch, src, srcPitch, width, height, scale, FUNC) do { \
    uint8_t* dest_ = (dest); \
    uint8_t const* src_ = (src); \
    int destPitch_ = (destPitch); \
    int srcPitch_ = (srcPitch); \
    int width_ = (width); \
    int height_ = (height); \
    int scale_ = (scale); \
    uint8_t R1, R2, R3, R4, R5, R6, R7, R8, R9; \
    uint8_t A, B, C, D, E, F, G, H, I; \
    /* First line */ \
    { \
        uint8_t const* line_  = src_; \
        uint8_t const* lineB_ = src_ + srcPitch_; \
        uint8_t* destLine_  = dest_; \
        A = 0; B = 0        ; C = 0        ; \
        D = 0; E = line_ [0]; F = line_ [1]; \
        G = 0; H = lineB_[0]; I = lineB_[1]; \
        /* First pixel on first line */ \
        FUNC(); \
        PIX_X(destLine_, scale_); \
        SHIFT_X(); \
        ++line_; ++lineB_; \
        for(int x = 1; x < width - 1; ++x) { \
            ++line_; ++lineB_; \
            F = *line_; \
            I = *lineB_; \
            destLine_ += scale_; \
            FUNC(); \
            PIX_X(destLine_, scale_); \
            SHIFT_X(); \
        } \
        C = 0; F = 0; I = 0; \
        /* Last pixel on first line */ \
        FUNC(); \
        PIX_X(destLine_ + 1, scale_); \
    } \
    for(int y = 1; y < height - 1; ++y) \
    { \
        uint8_t const* lineA_ = src_ + (y-1)*srcPitch_; \
        uint8_t const* line_  = src_ + y*srcPitch_; \
        uint8_t const* lineB_ = src_ + (y+1)*srcPitch; \
        uint8_t* destLine_  = dest_ + scale_*y*destPitch; \
        A = 0; B = lineA_[0]; C = lineA_[1]; \
        D = 0; E = line_ [0]; F = line_ [1]; \
        G = 0; H = lineB_[0]; I = lineB_[1]; \
        /* First pixel */ \
        FUNC(); \
        PIX_X(destLine_, scale_); \
        SHIFT_X(); \
        ++lineA_; ++line_; ++lineB_; \
        for(int x = 1; x < width - 1; ++x) { \
            ++lineA_; ++line_; ++lineB_; \
            C = *lineA_; \
            F = *line_; \
            I = *lineB_; \
            destLine_ += scale_; \
            FUNC(); \
            PIX_X(destLine_, scale_); \
            SHIFT_X(); \
        } \
        /* Last pixel */ \
        C = 0; F = 0; I = 0; \
        FUNC(); \
        PIX_X(destLine_ + 1, scale_); \
    } \
    /* Last line */ \
    { \
        uint8_t const* lineA_ = src_ + (height-2)*srcPitch_; \
        uint8_t const* line_  = src_ + (height-1)*srcPitch_; \
        uint8_t* destLine_  = dest_ + scale_*(height-1)*destPitch; \
        A = 0; B = lineA_[0]; C = lineA_[1]; \
        D = 0; E = line_ [0]; F = line_ [1]; \
        G = 0; H = 0        ; I = 0        ; \
        /* First pixel on last line */ \
        FUNC(); \
        PIX_X(destLine_, scale_); \
        SHIFT_X(); \
        ++lineA_; ++line_; \
        for(int x = 1; x < width - 1; ++x) { \
            ++lineA_; ++line_; \
            C = *lineA_; \
            F = *line_; \
            destLine_ += scale_; \
            FUNC(); \
            PIX_X(destLine_, scale_); \
            SHIFT_X(); \
        } \
        /* Last pixel on last line */ \
        C = 0; F = 0; I = 0; \
        FUNC(); \
        PIX_X(destLine_ + 1, scale_); \
    } \
} while(0)

#endif // UUID_16A8D91C6BEA4174A45E11A5F85FB93C
