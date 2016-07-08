#ifndef UUID_E1CFA019B03E443B1659DFA11E160066
#define UUID_E1CFA019B03E443B1659DFA11E160066

#define TL_DEF_RECT(T, name) \
typedef struct name { \
	T x1, y1, x2, y2; \
} name;

TL_DEF_RECT(int, tl_recti)

#define tl_rect_width(r)  ((r).x2 - (r).x1)
#define tl_rect_height(r) ((r).y2 - (r).y1)

#endif // UUID_E1CFA019B03E443B1659DFA11E160066
