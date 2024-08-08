#include <udis86.h>

#include "tyvm.h"
#include "asm.h"
#include <string.h>
#include <tl/vector.h>

void dump_asm(u8* beg, u8* end);

typedef struct tyvm_module {
	u8* org_stack_ptr;
} tyvm_module;

#define MAX_INSTR (16)

u8* align_mc(u8* LOC) {
	u8 *p = LOC, *end;
	while (((int)LOC) & 31) {
		c_nop();
	}
	if (LOC - p > 3) {
		end = LOC;
		LOC = p;
		c_jmp8(end - (p + 2));
		LOC = end;
	}
	return LOC;
}

#define WRITE_INSTR(instr) do { u8* _p = LOC; instr; \
	if ((((int)_p) ^ ((int)LOC - 1)) & ~31) { \
		LOC = align_mc(_p); \
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
	OP_JMP,
	OP_LOADIP
};

static int REG(int n) {
	return n < 3 ? RAX + n : (n - 3) + R8;
}

#define READ_REGOPS() u8 _opr = *code++; \
	u8 R = _opr & 0x7; \
	u8 SR = (_opr >> 4) & 0x7;

#define WRITE_REGOPS(r, sr) (*P->dest++ = ((sr) << 4) | (r))

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
				((u32*)p->loc)[-1] = (u32)(any_labels[p->to] - mem); \
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

	WRITE_INSTR(c_push_q(R12));
	WRITE_INSTR(c_push_q(RBX));
	WRITE_INSTR(c_push_q(RSI));

	WRITE_INSTR(c_mov_rk64(MEMBASE, 0));
	data_ptr = LOC;

	WRITE_INSTR(c_mov_rk64(RCX, (u64)&M->org_stack_ptr));
	WRITE_INSTR(c_wide(); c_mov_r(m_si_d32(RSP, RCX, SS_1, 0)));

	for (i = 0; i < 8; ++i) {
		WRITE_INSTR(c_xor(rq(REG(i), REG(i))));
	}

	WRITE_INSTR(c_mov_rk64(RSP, (u64)mem));

	for (; code + MAX_INSTR <= code_end; ) {
		u8 op = *code++;
		u8 label = op >> 7;
		op = op & 0x7f;
		if (label) {
			// Align to 32-byte
			LOC = align_mc(LOC);
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
		case OP_JMP: {
			u32 k, addr_reg;
			i32 op = *code++;
			int indirect = (op >> 4) & 1;

			if (indirect) {
				addr_reg = op >> 5;
			} else {
				code = read_const(code, &k);
			}

			if (indirect) {
				WRITE_INSTR(c_and_k(r(addr_reg), 0xffffff80));
				WRITE_INSTR(c_lea(mq_sib(RSI, MEMBASE, addr_reg, SS_1)));
				WRITE_INSTR(c_jmp_ind(r(RSI)));
			} else if (k < any_label_next - any_labels) {
				WRITE_INSTR(c_jmp(any_labels[k]));
			} else {
				WRITE_INSTR(c_jmp_far(0));
				PENDING_LABEL(k);
			}
			break;
		}
		case OP_JCND: {
			u32 k, addr_reg;
			i32 op = *code++;
			int indirect = (op >> 4) & 1;
			READ_REGOPS();

			WRITE_INSTR(c_cmp(r(REG(R), REG(SR))));

			if (indirect) {
				addr_reg = op >> 5;
			} else {
				code = read_const(code, &k);
			}

			op &= 0xf;

			if (indirect) {
				u8* patch;
				WRITE_INSTR(c_jcnd8(JCND_JO + (op ^ 1), 0));
				patch = LOC;
				WRITE_INSTR(c_and_k(r(addr_reg), 0xffffff80));
				WRITE_INSTR(c_lea(mq_sib(RSI, MEMBASE, addr_reg, SS_1)));
				WRITE_INSTR(c_jmp_ind(r(RSI)));
				c_label8(patch);
			} else if (k < any_label_next - any_labels) {
				WRITE_INSTR(c_jcnd(JCND_JO + op, any_labels[k]));
			} else {
				WRITE_INSTR(c_jcnd_far(JCND_JO + op, 0));
				PENDING_LABEL(k);
			}
			break;
		}
		default:
			goto end_instr;
		}
	}
end_instr:

	WRITE_INSTR(c_mov_rk64(RCX, (u64)&M->org_stack_ptr));
	WRITE_INSTR(c_wide(); c_mov(m_si_d32(RSP, RCX, SS_1, 0)));
	WRITE_INSTR(c_pop_q(RSI));
	WRITE_INSTR(c_pop_q(RBX));
	WRITE_INSTR(c_pop_q(R12));

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

	if (0)
	{
		int v = ((int(*)())c)();

		printf("v = %d\n", v);
	}
}

#include <stdio.h>
#include <stdlib.h>

enum {
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

typedef struct parse_label {
	char const *name, *name_end;
	int is_code;
	uint32_t value;
} parse_label;

typedef struct pending_label_parse {
	u8* loc;
	char const *name, *name_end;
	int kind;
} pending_label_parse;

tl_def_vector(label_vec, parse_label)

typedef struct parser {
	int token, line;
	char const* cur;

	char const* tok_data;
	char const* tok_data_end;
	u32 tok_int_data;
	u8 tok_cnd;

	u8* dest;
	u8* dest_end;

	label_vec labels;

	struct pending_label_parse pending_labels[1024],
		*pending_labels_next,
		*pending_labels_end;
} parser;

#define KW4(a,b,c,d) (((a)<<24)+((b)<<16)+((c)<<8)+(d))
#define KW3(a,b,c) (((a)<<16)+((b)<<8)+(c))
#define KW2(a,b) (((a)<<8)+(b))

static void lex(parser* P) {
	char c;
repeat:
	c = *P->cur;

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
				case KW2('J', 'L'): P->token = OP_JCND; P->tok_cnd = JCND_JL; break;
				case KW3('J', 'M', 'P'): P->token = OP_JMP; break;

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

// TODO: Check that addr-labels are aligned

#define PENDING_LABEL_PARSE(_name, _name_end) do { \
	P->pending_labels_next->loc = P->dest; \
	P->pending_labels_next->name = (_name); \
	P->pending_labels_next->name_end = (_name_end); \
	P->pending_labels_next->kind = LABEL_JUMP; \
	if (++P->pending_labels_next == P->pending_labels_end) { \
		flush_labels_parse(P); \
	} \
} while (0)

#define WRITE_OP(n) (*P->dest++ = ((n) | ((next_label_pos == pos) << 7)))

void flush_labels_parse(parser* P) {
	struct pending_label_parse* p = P->pending_labels;
	for (; p != P->pending_labels_next; ) {
		tl_vector_foreach(P->labels.v, parse_label, l, {
			printf("len %d, first %c\n", l->name_end - l->name, l->name[0]);
			printf("len %d, first %c\n", p->name_end - p->name, p->name[0]);

			if (l->name_end - l->name == p->name_end - p->name
			 && memcmp(l->name, p->name, p->name_end - p->name) == 0) {
				printf("write %d\n", l->value);
				((u32*)p->loc)[-1] = l->value;
				--P->pending_labels_next;
				*p = *P->pending_labels_next;
				--p;
				break;
			}
		});
		++p;
	}
}

int r_reg(parser* P) {
	int t = P->token;
	if (t >= T_A && t <= T_J) {
		lex(P);
		return t - T_A;
	}
	parse_error(P);
}

void r_file(parser* P) {


	uint32_t pos = 0;
	uint32_t next_label_pos = 0xffffffff;
	while (P->token < 256 || P->token == T_NL || P->token == T_COLON || P->token == T_IDENT) {
		switch (P->token) {
		case T_SET: {
			int r;
			lex(P);
			r = r_reg(P);
			expect(P, T_COMMA);

			if (P->token == T_INT) {
				WRITE_OP(OP_LOADK32);
				*P->dest++ = r;

				*P->dest++ = (u8)P->tok_int_data;
				*P->dest++ = (u8)(P->tok_int_data >> 8);
				*P->dest++ = (u8)(P->tok_int_data >> 16);
				*P->dest++ = (u8)(P->tok_int_data >> 24);
				lex(P);
			} else if (P->token == T_IDENT) {
				WRITE_OP(OP_LOADIP);
				*P->dest++ = r;
				P->dest += 4;
				PENDING_LABEL_PARSE(P->tok_data, P->tok_data_end);
				lex(P);

				// TODO: Constants
			} else {
				parse_error(P);
			}
			++pos;
			break;
		}
		case T_IDENT: {
			parse_label l;
			l.name = P->tok_data;
			l.name_end = P->tok_data_end;
			l.value = pos;
			l.is_code = 1;
			label_vec_pushback(&P->labels, l);
			lex(P);
			expect(P, T_COLON);
			next_label_pos = pos;
			break;
		}
		case OP_JMP: {
			u32 op;

			WRITE_OP(OP_JMP);
			op = 0;
			lex(P);

			if (P->token == T_IDENT) {
				*P->dest++ = op;
				P->dest += 4;
				PENDING_LABEL_PARSE(P->tok_data, P->tok_data_end);
				lex(P);
			} else {
				int jr = r_reg(P);
				*P->dest++ = (jr << 5) | (1 << 4) | op;
			}

			break;
		}
		case OP_JCND: {
			int r, sr;
			u32 op;

			WRITE_OP(OP_JCND);
			op = P->tok_cnd - JCND_JO;
			lex(P);

			r = r_reg(P);
			expect(P, T_COMMA);
			sr = r_reg(P);
			expect(P, T_COMMA);
			if (P->token == T_IDENT) {
				*P->dest++ = op;
				WRITE_REGOPS(r, sr);
				P->dest += 4;
				PENDING_LABEL_PARSE(P->tok_data, P->tok_data_end);
				lex(P);
			} else {
				int jr = r_reg(P);
				*P->dest++ = (jr << 5) | (1 << 4) | op;
				WRITE_REGOPS(r, sr);
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
			WRITE_OP(op);
			*P->dest++ = r | (sr << 4);
			++pos;
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

	flush_labels_parse(P);

	if (P->pending_labels_next != P->pending_labels) {
		printf("%d labels unresolved\n", P->pending_labels_next - P->pending_labels);
	}

	expect(P, T_EOF);
}

u8* parse(char const* str) {
	parser P;

	u8* buf = malloc(4096);

	P.line = 1;
	P.cur = str;
	P.dest = buf;
	P.dest_end = buf + 4096;
	label_vec_init_empty(&P.labels);

	P.pending_labels_next = P.pending_labels;
	P.pending_labels_end = P.pending_labels + 1024;

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

	u8* instr = parse(
		"SET A, 10\n"
		"a: SET B, 20\n"
		"ADD A, B\n"
		"JL A, B, a\n"
		"SET C, a\n"
		"JMP C\n");
	tyvm_compile(&m, instr, instr + 4096);
	return 0;
}