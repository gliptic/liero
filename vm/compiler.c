#include <udis86.h>

#include "tyvm.h"
#include "asm.h"

void dump_asm(u8* beg, u8* end);

typedef struct tyvm_module {
	u8* org_stack_ptr;
} tyvm_module;

/*

*/

#define MAX_INSTR (4)

#define ALIGN_MC() do { while (((int)LOC) & 31) { c_nop(); } } while(0)

#define WRITE_INSTR(instr) do { u8* _p = LOC; instr; \
	if ((((int)_p) ^ ((int)LOC - 1)) & ~31) { \
		LOC = _p; ALIGN_MC(); \
	} else { break; } \
} while(1)

u8 const* read_const(u8 const* src, u32* r) {
	u32 v = *src++;
	v |= (u32)*src++ << 8;
	v |= (u32)*src++ << 16;
	v |= (u32)*src++ << 24;
	*r = v;
	return src;
}

u8 const* read_i32(u8 const* src, u32* r) {
	u32 v;
	src = read_const(src, &v);
	*r = (i32)v;
	return src;
}

enum {
	OP_LOADK32 = 0,
	OP_LOADINDIR32,
	OP_STOREINDIR32,
	OP_ADD,
	OP_JCND,
	OP_LOADIP
};

static int REG(int n) {
	return n < 3 ? RAX + n : (n - 3) + R8;
}

#define READ_REGOPS() u8 _opr = *code++; \
	u8 R = _opr & 0x7; \
	u8 SR = (_opr >> 4) & 0x7;

struct pending_label {
	u8* loc;
	u32 to;
	int kind;
};

#define TRACE(args) printf args
//#define TRACE(args) (void)0

#define TRACE2(args) (void)0

#define LABEL_JUMP (0)
#define LABEL_ADDR (1)

// TODO: Check that addr-labels are aligned

#define FLUSH_LABELS() do { \
	struct pending_label* p = pending_labels_arr; \
	for (; p != pending_labels; ) { \
		if (p->to < any_label_next - any_labels) { \
			TRACE(("resolving label to %d (%p)\n", p->to, any_labels[p->to])); \
			if (p->kind == LABEL_JUMP) { \
				c_label32_to(p->loc, any_labels[p->to]); \
			} else { \
				((u32*)p->loc)[-1] = (u32)(any_labels[p->to] - mem) >> 5; \
			} \
			--pending_labels; \
			*p = *pending_labels; \
		} else { \
			++p; \
		} \
	} \
} while (0)

#define PENDING_LABEL(_to) do { \
	pending_labels->loc = LOC; \
	pending_labels->to = (_to); \
	pending_labels->kind = LABEL_JUMP; \
	if (++pending_labels == pending_labels_end) { \
		FLUSH_LABELS(); \
	} \
} while (0)

#define PENDING_ADDR(_to) do { \
	pending_labels->loc = LOC; \
	pending_labels->to = (_to); \
	pending_labels->kind = LABEL_ADDR; \
	if (++pending_labels == pending_labels_end) { \
		FLUSH_LABELS(); \
	} \
} while (0)

#define MEMBASE EBX
#define stack_space (1 << 16)

void tyvm_compile(tyvm_module* M, u8 const* code, u8 const* code_end) {
	u8* mem = mem_reserve();
	u8* c = mem;
	u8* data = mem_commit_exec(c, 4096);
	u8* LOC = c, *data_ptr;
	u8* any_labels[1024], **any_label_next = any_labels;
	struct pending_label pending_labels_arr[1024],
		*pending_labels = pending_labels_arr,
		*pending_labels_end = pending_labels_arr + 1024;
	int i;

	c_push_q(R12);
	c_push_q(RBX);
	c_push_q(RSI);

	c_mov_rk64(MEMBASE, 0);
	data_ptr = LOC;
	
	c_mov_rk64(RCX, (u64)&M->org_stack_ptr);
	c_wide(); c_mov_r(m_si_d32(RSP, RCX, SS_1, 0));

	for (i = 0; i < 8; ++i) {
		c_xor(rq(REG(i), REG(i)));
	}
	
	c_mov_rk64(RSP, (u64)mem);

	for (; code + MAX_INSTR <= code_end; ) {
		u8 op = *code++;
		u8 label = op >> 7;
		op = op & 0x7f;
		if (label) {
			// Align to 32-byte
			ALIGN_MC();
		}
		*any_label_next++ = LOC;
		switch (op) {
		case OP_LOADK32: {
			u8 R = *code++ & 0x7;
			u32 k;
			code = read_const(code, &k);
			WRITE_INSTR(c_mov_rk(REG(R), k));
			break;
		}
		case OP_LOADINDIR32: {
			READ_REGOPS();
			WRITE_INSTR(c_mov(m_sib(REG(R), MEMBASE, REG(SR), SS_1)));
			break;
		}
		case OP_STOREINDIR32: {
			READ_REGOPS();
			WRITE_INSTR(c_mov_r(m_sib(REG(R), MEMBASE, REG(SR), SS_1)));
			break;
		}
		case OP_ADD: {
			READ_REGOPS();
			WRITE_INSTR(c_add(r(REG(R), REG(SR))));
			break;
		}
		case OP_LOADIP: {
			u8 R = *code++ & 0x7;
			u32 k;
			code = read_const(code, &k);
			WRITE_INSTR(c_mov_rk(REG(R), 0));
			PENDING_ADDR(k);
			break;
		}
		case OP_JCND: {
			u32 k, addr_reg;
			i32 op = *code++;
			int indirect = (op >> 4) & 1;
			READ_REGOPS();

			c_cmp(r(REG(R), REG(SR)));

			if (indirect) {
				addr_reg = op >> 5;
			} else {
				code = read_const(code, &k);
			}

			op &= 0xf;

			if (indirect) {
				u8* patch;
				c_jcnd8(JCND_JO + (op ^ 1), 0);
				patch = LOC;
				c_shr_k(r(addr_reg), 5);
				c_lea(mq_sib(RSI, MEMBASE, addr_reg, SS_1));
				c_jmp_ind(r(RSI));
				c_label8(patch);
			} else if (k < any_label_next - any_labels) {
				c_jcnd(JCND_JO + op, any_labels[k]);
			} else {
				c_jcnd_far(JCND_JO + op, 0);
				PENDING_LABEL(k);
			}
			break;
		}
		default:
			goto end_instr;
		}
	}
end_instr:

	c_mov_rk64(RCX, (u64)&M->org_stack_ptr);
	c_wide(); c_mov(m_si_d32(RSP, RCX, SS_1, 0));
	c_pop_q(RSI);
	c_pop_q(RBX);
	c_pop_q(R12);

	WRITE_INSTR(c_retn());

	FLUSH_LABELS();

	{
		u8* stack_end;
		mem_commit_data(data, 1 << 16);
		stack_end = mem_commit_data(mem + (1ull << 32) - stack_space, stack_space);

		((u64*)data_ptr)[-1] = (u64)data;
	}
	

	// TODO: Lock down machine code from writing

	dump_asm(c, LOC);
	printf("DONE\n");

	{
		int v = ((int(*)())c)();

		printf("v = %d\n", v);
	}
}

#include <stdio.h>
#include <stdlib.h>

enum {
	/*
	T_POP = VAL_POP,
	T_PEEK = VAL_PEEK,
	T_PUSH = VAL_PUSH,
	T_SP = VAL_SP,
	T_PC = VAL_PC,
	T_O = VAL_O,
	T_JSR,
	*/
	T_SET = 128,


	T_IDENT = 256,
	T_INT, T_COMMA, T_PLUS,
	T_LBRACKET, T_RBRACKET,
	T_COLON, T_NL,

	T_A, T_B, T_C,
	T_X, T_Y, T_Z,
	T_I, T_J,

	T_EOF
};

typedef struct parser {
	int token, line;
	char const* cur;

	char const* tok_data;
	char const* tok_data_end;
	u32 tok_int_data;

	u8* dest;
	u8* dest_end;
} parser;

#define KW4(a,b,c,d) (((a)<<24)+((b)<<16)+((c)<<8)+(d))
#define KW3(a,b,c) (((a)<<16)+((b)<<8)+(c))
#define KW2(a,b) (((a)<<8)+(b))

static void lex(parser* P) {
	char c;
repeat:
	c = *P->cur;
	if (!c) {
		P->token = T_EOF;
		return;
	}

	switch (c) {
	case '\0':
		P->token = T_EOF;
		return;
	case '[': ++P->cur; P->token = T_LBRACKET; break;
	case ']': ++P->cur; P->token = T_RBRACKET; break;
	case ':': ++P->cur; P->token = T_COLON; break;
	case ',': ++P->cur; P->token = T_COMMA; break;
	case '+': ++P->cur; P->token = T_PLUS; break;
	case '\n': ++P->cur; P->token = T_NL; ++P->line; break;

	case '\r': case ' ': case '\t':
		++P->cur;
		goto repeat;

	default: {
		if ((c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')) {
			u32 kw = 0;
			P->tok_data = P->cur;

			do {
				kw = (kw << 8) + c;
				c = *++P->cur;
			} while ((c >= 'a' && c <= 'z')
				|| (c >= 'A' && c <= 'Z'));

			P->tok_data_end = P->cur;
			P->token = T_IDENT;

			if ((P->tok_data_end - P->tok_data) <= 4) {
				switch (kw) {
				case KW3('S', 'E', 'T'): P->token = T_SET; break;
				case KW3('A', 'D', 'D'): P->token = OP_ADD; break;
				//case KW3('S', 'U', 'B'): P->token = OP_SUB; break;
				//case KW3('J', 'S', 'R'): P->token = T_JSR; break;
				//case KW3('I', 'F', 'E'): P->token = OP_IFE; break;
				//case KW3('I', 'F', 'G'): P->token = OP_IFG; break;
				//case KW4('P', 'U', 'S', 'H'): P->token = T_PUSH; break;
				//case KW4('P', 'E', 'E', 'K'): P->token = T_PEEK; break;
				//case KW3('P', 'O', 'P'): P->token = T_POP; break;
				//case KW2('P', 'C'): P->token = T_PC; break;
				//case KW2('S', 'P'): P->token = T_SP; break;
				//case 'O': P->token = T_O; break;

				case 'A': P->token = T_A; break;
				case 'B': P->token = T_B; break;
				case 'C': P->token = T_C; break;
				case 'X': P->token = T_X; break;
				case 'Y': P->token = T_Y; break;
				case 'Z': P->token = T_Z; break;
				case 'I': P->token = T_I; break;
				case 'J': P->token = T_J; break;
				}
			}
		} else if (c >= '0' && c <= '9') {
			u16 i = 0;
			P->tok_data = P->cur;

			while (c >= '0' && c <= '9') {
				i = (i * 10) + (c - '0');
				c = *++P->cur;
			}

			P->tok_data_end = P->cur;
			P->tok_int_data = i;
			P->token = T_INT;
		}
	}
	}
}

#define W16(w) { *P->dest++ = (w); }

static void parse_error(parser* P) {
	// TODO: Error
	printf("Error on line %d\n", P->line);
	abort();
}

static int test(parser* P, int tok) {
	if (P->token == tok) {
		lex(P);
		return 1;
	}

	return 0;
}

static void expect(parser* P, int tok) {
	if (P->token != tok) {
		parse_error(P);
		return;
	}

	lex(P);
}

/*
static u16 r_operand(parser* P) {
	u16 v = (u16)P->token;
	switch (v) {
	case T_INT: {
		u16 i = (u16)P->tok_int_data;
		lex(P);
		if (i < 0x20) {
			return i + 0x20;
		} else {
			W16(i);
			return VAL_LIT;
		}
	}

	case T_POP: case T_PEEK: case T_PUSH:
	case T_SP: case T_PC: case T_O:
		lex(P);
		return v;

	case T_LBRACKET: {
		int used = 0;

		lex(P);
		v = 0x1e;

		do {
			if (!(used & 1) && P->token >= T_A && P->token <= T_J) {
				used |= 1;
				v = 0x8 + P->token - T_A;
				lex(P);
			} else if (!(used & 2) && P->token == T_INT) {
				used |= 2;
				W16((u16)P->tok_int_data);
				lex(P);
			} else {
				parse_error(P);
			}
		} while (test(P, T_PLUS));

		expect(P, T_RBRACKET);
		return (used == 3) ? v + 0x8 : v;
	}

	default: {
		if (v >= T_A && v <= T_J) {
			lex(P);
			return v - T_A;
		} else {
			printf("Invalid operand\n");
			parse_error(P);
			return 0;
		}
	}
	}
}*/

int r_reg(parser* P) {
	int t = P->token;
	if (t >= T_A && t <= T_J) {
		lex(P);
		return t - T_A;
	}
	parse_error(P);
}

void r_file(parser* P) {
	while (P->token < 256 || P->token == T_NL || P->token == T_COLON) {
		switch (P->token) {
		case T_SET: {
			int r;
			lex(P);
			r = r_reg(P);
			expect(P, T_COMMA);
			if (P->token == T_INT) {
				*P->dest++ = OP_LOADK32;
				*P->dest++ = r;
				*P->dest++ = (u8)P->tok_int_data;
				*P->dest++ = (u8)(P->tok_int_data >> 8);
				*P->dest++ = (u8)(P->tok_int_data >> 16);
				*P->dest++ = (u8)(P->tok_int_data >> 24);
				lex(P);
			} else {
				// TODO: Labels/constants
				parse_error(P);
			}
			break;
		}
		case OP_ADD: {
			int r, sr;
			int op = P->token;
			lex(P);
			r = r_reg(P);
			expect(P, T_COMMA);
			sr = r_reg(P);
			*P->dest++ = op;
			*P->dest++ = r | (sr << 4);
			break;
		}
		case T_NL:
			// Ignore
			lex(P);
			break;
		default: {
			parse_error(P);
		}
		}
	}

	*P->dest++ = 127;

	expect(P, T_EOF);
}

u8* parse(char const* str) {
	parser P;

	u8* buf = malloc(4096);

	P.line = 1;
	P.cur = str;
	P.dest = buf;
	P.dest_end = buf + 4096;

	lex(&P);

	r_file(&P);

	return buf;
}


void dump_asm(u8* beg, u8* end) {
	ud_t ud_obj;

	ud_init(&ud_obj);
	ud_set_input_buffer(&ud_obj, (unsigned char*)beg, end - beg);
	ud_set_pc(&ud_obj, 0);
	ud_set_mode(&ud_obj, 64);
	ud_set_syntax(&ud_obj, UD_SYN_INTEL);

	while (ud_disassemble(&ud_obj)) {
		printf("%08x\t", (unsigned int)ud_insn_off(&ud_obj));
		printf("%s\n", ud_insn_asm(&ud_obj));
	}
}

int main() {
	tyvm_module m;
	/*
	u8 instr[128] = {
		OP_LOADK32, 0, 1, 0, 0, 1,
		OP_LOADINDIR32, 3 + (4 << 4),
		OP_STOREINDIR32, 3 + (4 << 4),
		OP_LOADIP, 1, 4, 0, 0, 0,
		OP_ADD + 128, 4 + (5 << 4),
		OP_JCND, 4 + (5 << 4), 16 + 0xc + (1 << 5),
		OP_ADD, 0 + (0 << 4),
		OP_ADD, 1 + (1 << 4),
		127
	};*/
	u8* instr = parse(
		"SET A, 10\n"
		"ADD A, B\n");
	tyvm_compile(&m, instr, instr + 4096);
	return 0;
}