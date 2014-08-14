#ifndef ASM_H
#define ASM_H

#include <stddef.h>
#include <inttypes.h>
#include <assert.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t  u8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int8_t   i8;

#define c_byte(b) *LOC++ = b
#define c_word(b) do { *((u16*)LOC) = (b); LOC += 2; } while(0)
#define c_dword(b) do { *((u32*)LOC) = (b); LOC += 4; } while(0)
#define c_qword(b) do { *((u64*)LOC) = (b); LOC += 8; } while(0)

#define EAX  (0)
#define ECX  (1)
#define EDX  (2)
#define EBX  (3)
#define ESP  (4)
#define EBP  (5)
#define ESI  (6)
#define EDI  (7)
#define R8D  (8)
#define R9D  (9)
#define R10D (10)
#define R11D (11)
#define R12D (12)
#define R13D (13)
#define R14D (14)
#define R15D (15)

#define RAX (0)
#define RCX (1)
#define RDX (2)
#define RBX (3)
#define RSP (4)
#define RBP (5)
#define RSI (6)
#define RDI (7)
#define R8  (8)
#define R9  (9)
#define R10 (10)
#define R11 (11)
#define R12 (12)
#define R13 (13)
#define R14 (14)
#define R15 (15)

#define AL  (0)
#define CL  (1)
#define DL  (2)
#define BL  (3)
#define AH  (4)
#define CH  (5)
#define DH  (6)
#define BH  (7)
#define R8B  (8)
#define R9B  (9)
#define R10B (10)
#define R11B (11)
#define R12B (12)
#define R13B (13)
#define R14B (14)
#define R15B (15)

#define AX  (0)
#define CX  (1)
#define DX  (2)
#define BX  (3)
#define SP  (4)
#define BP  (5)
#define SI  (6)
#define DI  (7)
#define R8W  (8)
#define R9W  (9)
#define R10W (10)
#define R11W (11)
#define R12W (12)
#define R13W (13)
#define R14W (14)
#define R15W (15)

#define RMASK (0x7)

#define RM_EAX (0)
#define RM_ECX (1)
#define RM_EDX (2)
#define RM_EBX (3)
#define RM_EBP (5)
#define RM_ESI (6)
#define RM_EDI (7)

#define RM_RAX (0)
#define RM_RCX (1)
#define RM_RDX (2)
#define RM_RBX (3)
#define RM_RBP (5)
#define RM_RSI (6)
#define RM_RDI (7)

#define RM_SIB (4)

// c_sib(SIBR_DISP, ...)
#define SIBR_DISP (5)

#define SIBMOD_DISP32 (0)

#define MOD_M    (0)
#define MOD_MD8  (1)
#define MOD_MD32 (2)
#define MOD_R    (3)

#define SS_1 (0)
#define SS_2 (1)
#define SS_4 (2)
#define SS_8 (3)

#define X86_ADD8_R   (0x00)
#define X86_ADD_R    (0x01)
#define X86_ADD8     (0x02)
#define X86_ADD      (0x03)
#define X86_ADC8_R   (0x10)
#define X86_ADC_R    (0x11)
#define X86_ADC8     (0x12)
#define X86_ADC      (0x13)
#define X86_SBB8_R   (0x18)
#define X86_SBB_R    (0x19)
#define X86_SBB8     (0x1A)
#define X86_SBB      (0x1B)
#define X86_SUB8_R   (0x28)
#define X86_SUB_R    (0x29)
#define X86_SUB8     (0x2A)
#define X86_SUB      (0x2B)
#define X86_XOR8_R   (0x30)
#define X86_XOR_R    (0x31)
#define X86_XOR8     (0x32)
#define X86_XOR      (0x33)
#define X86_CMP8_R   (0x38)
#define X86_CMP_R    (0x39)
#define X86_CMP8     (0x3A)
#define X86_CMP      (0x3B)
#define X86_TEST8    (0x84)
#define X86_TEST     (0x85)
#define X86_MOV8     (0x8A)
#define X86_MOV      (0x8B)
#define X86_LEA      (0x8D) // ?
#define X86_MOV8_R   (0x88)
#define X86_MOV_R    (0x89)
#define X86_RETN     (0xC3)
#define X86_CALLREL  (0xE8)

#define JCND_JO     (0x70)
#define JCND_JNO    (0x71)
#define JCND_JB     (0x72)
#define JCND_JAE    (0x73)
#define JCND_JZ     (0x74)
#define JCND_JNZ    (0x75)
#define JCND_JBE    (0x76)
#define JCND_JA     (0x77)
#define JCND_JS     (0x78)
#define JCND_JNS    (0x79)
#define JCND_JP     (0x7A)
#define JCND_JNP    (0x7B)
#define JCND_JL     (0x7C)
#define JCND_JGE    (0x7D)
#define JCND_JLE    (0x7E)
#define JCND_JG     (0x7F)

#define REX(r, b, x, w) (0x40 + (b) + 2*(x) + 4*(r) + 8*(w))

#define c_rex(r, b, x, w) do { \
	u8 _rex = REX(r,b,x,w); \
	if(_rex != 0x40) c_byte(_rex); \
} while(0)

#define c_check_index(index) do { assert(index != RSP); } while(0)

// r, [b + x*...]
// r, b
#define c_rex_for(r, b, x, w) c_rex((r)>>3, (b)>>3, (x)>>3, w)

#define c_wide() c_byte(REX(0, 0, 0, 1))

// For mod != MOD_R, rm cannot be ESP/RSP (RM_SIB replaces ESP/RSP)
#define c_modrm(mod, r, rm) c_byte(((mod)<<6)+((r)<<3)+(rm))
// index uses register constants
#define c_sib(base, index, ss) c_byte(((ss)<<6)+((index)<<3)+(base))

#define c_modm(r, rm)         c_modrm(MOD_M, (r)&RMASK, (rm)&RMASK)
#define c_modr(rd, rs)        c_modrm(MOD_R, (rd)&RMASK, (rs)&RMASK)
#define c_modm_d8(r, rm, d)   do { c_modrm(MOD_MD8, (r)&RMASK, (rm)&RMASK); c_byte(d); } while(0)
#define c_modm_d32(r, rm, d)  do { c_modrm(MOD_MD32, (r)&RMASK, (rm)&RMASK); c_dword(d); } while(0)
#define c_modmq(r, rm)        c_modm(r, rm)
#define c_modrq(rd, rs)       c_modr(rd, rs)
#define c_modmq_d8(r, rm, d)  c_modm_d8(r, rm, d)
#define c_modmq_d32(r, rm, d) c_modm_d32(r, rm, d)

#define c_modexm(rm)         c_modm(REXT, rm)
#define c_modexr(rs)         c_modr(REXT, rs)
#define c_modexm_d8(rm, d)   c_modm_d8(REXT, rm, d)
#define c_modexm_d32(rm, d)  c_modm_d32(REXT, rm, d)
#define c_modexmq(rm)        c_modmq(REXT, rm)
#define c_modexrq(rs)        c_modrq(REXT, rs)
#define c_modexmq_d8(rm, d)  c_modmq_d8(REXT, rm, d)
#define c_modexmq_d32(rm, d) c_modmq_d32(REXT, rm, d)

#define c_rex_modm(r, rm)         c_rex_for(r, rm, 0, 0)
#define c_rex_modr(rd, rs)        c_rex_for(rd, rs, 0, 0)
#define c_rex_modm_d8(r, rm, d)   c_rex_for(r, rm, 0, 0)
#define c_rex_modm_d32(r, rm, d)  c_rex_for(r, rm, 0, 0)
#define c_rex_modmq(r, rm)        c_rex_for(r, rm, 0, 1)
#define c_rex_modrq(rd, rs)       c_rex_for(rd, rs, 0, 1)
#define c_rex_modmq_d8(r, rm, d)  c_rex_for(r, rm, 0, 1)
#define c_rex_modmq_d32(r, rm, d) c_rex_for(r, rm, 0, 1)

#define c_rex_modexm(rm)          c_rex_for(0, rm, 0, 0)
#define c_rex_modexr(rs)          c_rex_for(0, rs, 0, 0)
#define c_rex_modexm_d8(rm, d)    c_rex_for(0, rm, 0, 0)
#define c_rex_modexm_d32(rm, d)   c_rex_for(0, rm, 0, 0)
#define c_rex_modexmq(rm)         c_rex_for(0, rm, 0, 1)
#define c_rex_modexrq(rs)         c_rex_for(0, rs, 0, 1)
#define c_rex_modexmq_d8(rm, d)   c_rex_for(0, rm, 0, 1)
#define c_rex_modexmq_d32(rm, d)  c_rex_for(0, rm, 0, 1)

// index cannot be RSP/ESP
// r, [base + index*ss + ...]
#define c_modm_sib(r, base, index, ss) do { \
	c_check_index(index); \
	if((base) == SIBR_DISP) { \
		/* Must encode with a zero-offset */ \
		c_modrm(MOD_MD8, (r)&RMASK, RM_SIB); c_sib((base)&RMASK, (index)&RMASK, (ss)); c_byte(0); \
	} else { \
		c_modrm(MOD_M,   (r)&RMASK, RM_SIB); c_sib((base)&RMASK, (index)&RMASK, (ss)); \
	} \
} while(0)

// r, [base + index*ss + d]
#define c_modm_sib_d8(r, base, index, ss, d) do { c_check_index(index); c_modrm(MOD_MD8, (r)&RMASK, RM_SIB); c_sib((base)&RMASK, (index)&RMASK, ss); c_byte(d); } while(0)
// r, [base + index*ss + d]
#define c_modm_sib_d32(r, base, index, ss, d) do { c_check_index(index); c_modrm(MOD_MD32, (r)&RMASK, RM_SIB); c_sib((base)&RMASK, (index)&RMASK, ss); c_dword(d); } while(0)
// r, [index*ss + d]
#define c_modm_si_d32(r, index, ss, d) do { c_modrm(SIBMOD_DISP32, (r)&RMASK, RM_SIB); c_sib(SIBR_DISP, (index)&RMASK, ss); c_dword(d); } while(0)

#define c_modexm_sib(base, index, ss) c_modm_sib(REXT, base, index, ss)
#define c_modexm_sib_d8(base, index, ss, d) c_modm_sib_d8(REXT, base, index, ss, d)
#define c_modexm_sib_d32(base, index, ss, d) c_modm_sib_d32(REXT, base, index, ss, d)
#define c_modexm_si_d32(index, ss,d ) c_modm_si_d32(REXT, index, ss, d)

#define c_modmq_sib(r, base, index, ss) c_modm_sib(r, base, index, ss)
#define c_modmq_sib_d8(r, base, index, ss, d) c_modm_sib_d8(r, base, index, ss)
#define c_modmq_sib_d32(r, base, index, ss, d) c_modm_sib_d32(r, base, index, ss)
#define c_modmq_si_d32(r, index, ss, d) c_modm_si_d32(r, index, ss)

#define c_rex_modm_sib(r, base, index, ss) c_rex_for(r, base, index, 0)
#define c_rex_modm_sib_d8(r, base, index, ss, d) c_rex_for(r, base, index, 0)
#define c_rex_modm_sib_d32(r, base, index, ss, d) c_rex_for(r, base, index, 0)
#define c_rex_modm_si_d32(r, index, ss, d) c_rex_for(r, 0, index, 0)
#define c_rex_modmq_sib(r, base, index, ss) c_rex_for(r, base, index, 1)
#define c_rex_modmq_sib_d8(r, base, index, ss, d) c_rex_for(r, base, index, 1)
#define c_rex_modmq_sib_d32(r, base, index, ss, d) c_rex_for(r, base, index, 1)
#define c_rex_modmq_si_d32(r, index, ss, d) c_rex_for(r, 0, index, 1)

#define c_rex_modexm_sib(base, index, ss) c_rex_for(0, base, index, 0)
#define c_rex_modexm_sib_d8(base, index, ss, d) c_rex_for(0, base, index, 0)
#define c_rex_modexm_sib_d32(base, index, ss, d) c_rex_for(0, base, index, 0)
#define c_rex_modexm_si_d32(index, ss, d) c_rex_for(0, 0, index, 0)
#define c_rex_modexmq_sib(base, index, ss) c_rex_for(0, base, index, 1)
#define c_rex_modexmq_sib_d8(base, index, ss, d) c_rex_for(0, base, index, 1)
#define c_rex_modexmq_sib_d32(base, index, ss, d) c_rex_for(0, base, index, 1)
#define c_rex_modexmq_si_d32(index, ss, d) c_rex_for(0, 0, index, 1)

#define c_adsize() c_byte(0x67)
#define c_opsize() c_byte(0x66)

#define c_op16 c_opsize

// _r: reverse (destination is memory)
// _k: literal
#define c_movzx_b(modrm) do { c_rex_mod##modrm; c_word(0xB60F); c_mod##modrm; } while(0)
#define c_movzx_w(modrm) do { c_rex_mod##modrm; c_word(0xB70F); c_mod##modrm; } while(0)

#define c_1b_modrm(b, modrm) do { c_rex_mod##modrm; c_byte(b); c_mod##modrm; } while(0)

#define c_mov(modrm)      c_1b_modrm(X86_MOV, modrm)
#define c_mov16(modrm)    do { c_op16(); c_1b_modrm(X86_MOV, modrm); } while(0)
#define c_mov8(modrm)     c_1b_modrm(X86_MOV8, modrm)
#define c_mov_r(modrm)    c_1b_modrm(X86_MOV_R, modrm)
#define c_mov16_r(modrm)  do { c_op16(); c_1b_modrm(X86_MOV_R, modrm); } while(0)
#define c_mov8_r(modrm)   c_1b_modrm(X86_MOV8_R, modrm)

#define c_add(modrm)        c_1b_modrm(X86_ADD, modrm)
#define c_add16(modrm)      do { c_op16(); c_1b_modrm(X86_ADD, modrm); } while(0)
#define c_add8(modrm)       c_1b_modrm(X86_ADD8, modrm)
#define c_add_r(modrm)      c_1b_modrm(X86_ADD_R, modrm)
#define c_add16_r(modrm)    do { c_op16(); c_1b_modrm(X86_ADD_R, modrm); } while(0)
#define c_add8_r(modrm)     c_1b_modrm(X86_ADD8_R, modrm)

#define c_adc(modrm)        c_1b_modrm(X86_ADC, modrm)
#define c_adc16(modrm)      do { c_op16(); c_1b_modrm(X86_ADC, modrm); } while(0)
#define c_adc8(modrm)       c_1b_modrm(X86_ADC8, modrm)
#define c_adc_r(modrm)      c_1b_modrm(X86_ADC_R, modrm)
#define c_adc16_r(modrm)    do { c_op16(); c_1b_modrm(X86_ADC_R, modrm); } while(0)
#define c_adc8_r(modrm)     c_1b_modrm(X86_ADC8_R, modrm)

#define c_sbb(modrm)        c_1b_modrm(X86_SBB, modrm)
#define c_sbb16(modrm)      do { c_op16(); c_1b_modrm(X86_SBB, modrm); } while(0)
#define c_sbb8(modrm)       c_1b_modrm(X86_SBB8, modrm)
#define c_sbb_r(modrm)      c_1b_modrm(X86_SBB_R, modrm)
#define c_sbb16_r(modrm)    do { c_op16(); c_1b_modrm(X86_SBB_R, modrm); } while(0)
#define c_sbb8_r(modrm)     c_1b_modrm(X86_SBB8_R, modrm)

#define c_sub(modrm)        c_1b_modrm(X86_SUB, modrm)
#define c_sub16(modrm)      do { c_op16(); c_1b_modrm(X86_SUB, modrm); } while(0)
#define c_sub8(modrm)       c_1b_modrm(X86_SUB8, modrm)
#define c_sub_r(modrm)      c_1b_modrm(X86_SUB_R, modrm)
#define c_sub16_r(modrm)    do { c_op16(); c_1b_modrm(X86_SUB_R, modrm); } while(0)
#define c_sub8_r(modrm)     c_1b_modrm(X86_SUB8_R, modrm)

#define c_xor(modrm)        c_1b_modrm(X86_XOR, modrm)
#define c_xor16(modrm)      do { c_op16(); c_1b_modrm(X86_XOR, modrm); } while(0)
#define c_xor8(modrm)       c_1b_modrm(X86_XOR8, modrm)
#define c_xor_r(modrm)      c_1b_modrm(X86_XOR_R, modrm)
#define c_xor16_r(modrm)    do { c_op16(); c_1b_modrm(X86_XOR_R, modrm); } while(0)
#define c_xor8_r(modrm)     c_1b_modrm(X86_XOR8_R, modrm)

#define c_cmp(modrm)        c_1b_modrm(X86_CMP, modrm)
#define c_cmp16(modrm)      do { c_op16(); c_1b_modrm(X86_CMP, modrm); } while(0)
#define c_cmp8(modrm)       c_1b_modrm(X86_CMP8, modrm)
#define c_cmp_r(modrm)      c_1b_modrm(X86_CMP_R, modrm)
#define c_cmp16_r(modrm)    do { c_op16(); c_1b_modrm(X86_CMP_R, modrm); } while(0)
#define c_cmp8_r(modrm)     c_1b_modrm(X86_CMP8_R, modrm)

#define c_lea(modrm)        c_1b_modrm(X86_LEA, modrm)

#define c_test(modrm)   c_1b_modrm(X86_TEST, modrm)
#define c_test8(modrm)  c_1b_modrm(X86_TEST8, modrm)

#define c_retn() c_byte(X86_RETN)
#define c_callrel(p) do { c_byte(X86_CALLREL); c_dword((u8*)p - (LOC + 4)); } while(0)

#define c_push_q(r) do { u32 _r = (r); c_rex_for(0, _r, 0, 0); c_byte(0x50 + (_r&RMASK)); } while(0)
#define c_pop_q(r) do { u32 _r = (r); c_rex_for(0, _r, 0, 0); c_byte(0x58 + (_r&RMASK)); } while(0)

#define c_mov_rk(r, k) do { u32 _r = (r); c_rex_for(0, _r, 0, 0); c_byte(0xB8 + (_r&RMASK)); c_dword(k); } while(0)
#define c_mov_rk64(r, k) do { u32 _r = (r); c_wide(); c_rex_for(0, _r, 0, 0); c_byte(0xB8 + (_r&RMASK)); c_qword(k); } while(0)

#define c_add_k8(modrm, k8) do { u32 REXT = 0; c_rex_modex##modrm; c_byte(0x83); c_modex##modrm; c_byte(k8); } while(0)
#define c_or_k8(modrm, k8) do { u32 REXT = 1; c_rex_modex##modrm; c_byte(0x83); c_modex##modrm; c_byte(k8); } while(0)

#define c_cmp8_k(modrm, k8) do { u32 REXT = 7; c_rex_modex##modrm; c_byte(0x80); c_modex##modrm; c_byte(k8); } while(0)

#define c_add16_k(modrm, k16) do { u32 REXT = 0; c_op16(); c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_word(k16); } while(0)
#define c_sub16_k(modrm, k16) do { u32 REXT = 5; c_op16(); c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_word(k16); } while(0)
#define c_cmp16_k(modrm, k16) do { u32 REXT = 7; c_op16(); c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_word(k16); } while(0)
#define c_mov16_k(modrm, k16) do { u32 REXT = 0; c_op16(); c_rex_modex##modrm; c_byte(0xC7); c_modex##modrm; c_word(k16); } while(0)

#define c_inc16(modrm) do { u32 REXT = 0; c_op16(); c_rex_modex##modrm; c_byte(0xFF); c_modex##modrm; } while(0)
#define c_dec16(modrm) do { u32 REXT = 1; c_op16(); c_rex_modex##modrm; c_byte(0xFF); c_modex##modrm; } while(0)

#define c_add_k(modrm, k) do { u32 REXT = 0; c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_dword(k); } while(0)
#define c_and_k(modrm, k) do { u32 REXT = 4; c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_dword(k); } while(0)
#define c_sub_k(modrm, k) do { u32 REXT = 5; c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_dword(k); } while(0)
#define c_cmp_k(modrm, k) do { u32 REXT = 7; c_rex_modex##modrm; c_byte(0x81); c_modex##modrm; c_dword(k); } while(0)
#define c_mov_k(modrm, k) do { u32 REXT = 0; c_rex_modex##modrm; c_byte(0xC7); c_modex##modrm; c_dword(k); } while(0)
#define c_mov_k64(modrm, k) do { u32 REXT = 0; c_wide(); c_rex_modex##modrm; c_byte(0xC7); c_modex##modrm; c_qword(k); } while(0)

#define c_shr_k(modrm, k) do { u32 REXT = 5; c_rex_modex##modrm; c_byte(0xC1); c_modex##modrm; c_byte(k); } while(0)

#define c_inc(modrm) do { u32 REXT = 0; c_rex_modex##modrm; c_byte(0xFF); c_modex##modrm; } while(0)
#define c_dec(modrm) do { u32 REXT = 1; c_rex_modex##modrm; c_byte(0xFF); c_modex##modrm; } while(0)

#define c_jmp_ind(modrm) do { u32 REXT = 4; c_rex_modex##modrm; c_byte(0xFF); c_modex##modrm; } while(0)

#define c_jz8(o)   do { c_byte(0x74); c_byte(o); } while(0)
#define c_jnz8(o)  do { c_byte(0x75); c_byte(o); } while(0)
#define c_jbe8(o)  do { c_byte(0x76); c_byte(o); } while(0)

#define c_jz(o)   do { c_word(0x840F); c_dword(o); } while(0)
#define c_jnz(o)  do { c_word(0x850F); c_dword(o); } while(0)
#define c_jbe(o)  do { c_word(0x860F); c_dword(o); } while(0)

#define c_nop()   c_byte(0x90)

#define c_label8(l) do { ((i8*)(l))[-1] = (LOC - (l)); } while(0)
#define c_label32(l) do { ((i32*)(l))[-1] = (LOC - (l)); } while(0)
#define c_label32_to(l, to) do { ((i32*)(l))[-1] = ((to) - (l)); } while(0)

#define c_jcnd8(cnd, o)   do { c_byte(cnd); c_byte(o); } while(0)
#define c_jcnd_far(cnd, pos) do { c_word(0x100F + ((cnd) << 8)); c_dword((u32)((u8*)(pos) - (LOC + 4))); } while(0)

#define c_jmp_far(pos) do { c_byte(0xE9); c_dword((u32)((u8*)(pos) - (LOC + 4))); } while(0)

#define c_jcnd(cnd, pos) { \
	u8* _pos = (u8*)(pos); \
	ptrdiff_t _diff = ((u8*)(pos) - (LOC + 2)); \
	if(_diff >= -128 && _diff <= 127) { \
		c_jcnd8(cnd, (i8)_diff); \
	} else { \
		c_jcnd_far(cnd, _pos); \
	} \
} while(0)

#define c_jmp8(o) do { c_byte(0xEB); c_byte(o); } while(0)

#define c_jmp(pos) do { \
	u8* _pos = (u8*)(pos); \
	ptrdiff_t _diff = ((u8*)(pos) - (LOC + 2)); \
	if(_diff >= -128 && _diff <= 127) { \
		c_byte(0xEB); c_byte(_diff); \
	} else { \
		c_byte(0xE9); c_dword(_diff - 3); \
	} \
} while(0)


void* mcode_alloc(size_t sz, int prot);
void* mem_commit_exec(void* p, size_t s);
void* mem_commit_data(void* p, size_t s);
void* mem_reserve();

#endif // ASM_H
