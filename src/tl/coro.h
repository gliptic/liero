#ifndef UUID_8F86DF4B2B144E1252F43DAEDF00FF59
#define UUID_8F86DF4B2B144E1252F43DAEDF00FF59

#define BEGIN_YIELDABLE() switch ((self)->yield_state) { case 0:;
#define END_YIELDABLE() }
#define YIELD_STATE() int yield_state;
#define YIELD_INIT(self) (self)->yield_state = 0
#define YIELD(n) do { (self)->yield_state = n; return 1; case n: ; } while(0)
#define YIELD_WITH(n, r) do { (self)->yield_state = n; return (r); case n: ; } while(0)
#define YIELD_WE(n, r) do { int r_ = (r); if (r_) YIELD_WITH(n, r_); else break; } while(1)
#define YIELD_WHILE(n, c) do { int c_ = (c); if (c_) YIELD(n); else break; } while(1)

#endif // UUID_8F86DF4B2B144E1252F43DAEDF00FF59
