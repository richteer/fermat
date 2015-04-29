#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "scope.h"
#include "gencode.h"
#include "parser.tab.h"

#define CASE0(t) (!t->left && !t->right && (t->rank == 1))
#define CASE1(t) (t->right && (t->right->rank == 0))
#define CASE2(t) (t->left && t->right && (t->left->rank < t->right->rank) && (t->left->rank >= 1))
#define CASE3(t) (t->left && t->right && (t->left->rank >= t->right->rank) && (t->right->rank >= 1))

#define GENSWITCH(t) switch((CASE0(t)) ? 0 : (CASE1(t)) ? 1 : (CASE2(t)) ? 2 : (CASE3(t)) ? 3 : 4)

extern FILE * outsrc;
extern scope_t * top;

char registers[][5] = {
	"%r10",
	"%r11",
	"%r12",
	"%r13",
	"%r14",
	"%r15",
};

typedef struct reg_s {
	int num;
	struct reg_s * next;
} reg_t;

typedef struct {
	reg_t * top;
} stack_t;

stack_t st;
int loopnum = 0;

static void reg_init(void)
{
	reg_t * r;
	int i;
	st.top = NULL;
	// TODO: Fix this
	for (i = 5; i >= 0; --i) {
		r = st.top;
		st.top = malloc(sizeof(reg_t));
		st.top->num = i;
		st.top->next = r;
	}

	return;
}

static void reg_deinit(void)
{
	reg_t *r, *tmp;
	for (r = st.top; r != NULL;) {
		tmp = r;
		r = r->next;
		free(tmp);
	}
}

static reg_t * reg_pop(void)
{
	reg_t * r;

	r = st.top;
	st.top = st.top->next;

	return r;
}

static void reg_push(reg_t * r)
{
	reg_t * t;

	t = st.top;
	st.top = r;
	r->next = t;

	return;
}

static void reg_swap(void)
{
	reg_t * n;

	n = st.top->next;
	st.top->next = n->next;
	n->next = st.top;
	st.top = n;

	return;
}


static int gen_rankify(tree_t * t)
{
	assert(t);
	if (!(t->left || t->right)) {
		return 0;
	}

	if (t->left && !t->left->left && !t->left->right) {
		t->left->rank = 1;
		printf("%d = %d\n", t->left->type, t->left->rank);
	}
	if (t->right && !t->right->left && !t->right->right) {
		t->right->rank = 0;
		printf("%d = %d\n", t->right->type, t->right->rank);
	}

	gen_rankify(t->left);
	gen_rankify(t->right);

	if (t->left->rank == t->right->rank) {
		t->rank = t->left->rank + 1;
		printf("%d = %d\n", t->type, t->rank);
	}
	else {
		t->rank = (t->left->rank < t->right->rank) ?
			t->right->rank : t->left->rank;
		printf("%d = %d\n", t->type, t->rank);
	}

	return 0;
}

int gen_preamble(void)
{
	spew(".LC0:\n");
	spew("\t.string \"%%ld\\n\"\n");
	spew(".LC1:\n");
	spew("\t.string \"%%ld\"\n");
	spew("\t.globl main\n");

	return 0;
}

int gen_postamble(char * name)
{
	spew("main:\n");
	spew("\tpushq\t%%rbp\n");
	spew("\tmovq\t%%rsp, %%rbp\n");
	spew("\tcall\t%s\n", name);
	gen_outro();

	return 0;
}

int gen_intro(char * name)
{
	spew("%s:\n", name);
	spew("\tpushq\t%%rbp\n");
	spew("\tmovq\t%%rsp, %%rbp\n");

	return 0;
}

int gen_outro(void)
{
	spew("\tleave\n");
	spew("\tret\n");

	return 0;
}

int gen_stalloc(int off)
{
	if (!off) return 0;

	spew("\tsubq\t$%d, %%rsp\n", -off);

	return 0;
}
int gen_dealloc(int off)
{
	if (!off) return 0;

	spew("\taddq\t$%d, %%rsp\n", -off);

	return 0;
}

int spew_jmp(char * jmp, int offset)
{
	char foo[10] = {0};

	sprintf(foo, ".LL%d", loopnum+offset);
	spew("\t%s\t%s\n",jmp,foo);

	return 0;
}

int gen_label(void)
{
	spew(".LL%d:\n",loopnum++);
	return 0;
}

int gen_jmp(tree_t * t)
{

	switch(t->attribute.opval) {
		case EQ: spew_jmp("je ",0); break;
		case NE: spew_jmp("jne",0); break;
		case LT: spew_jmp("jl ",0); break;
		case LE: spew_jmp("jle",0); break;
		case GT: spew_jmp("jg ",0); break;
		case GE: spew_jmp("jge",0); break;
		default: fprintf(stderr, "Wat\n"); break;
	}

	spew_jmp("jmp",1);

	return 0;
}

static void spew_id(char * fmt, tree_t * t, char * opt)
{
	int i;
	char foo[16] = {0};

	if (t->type == INUM) {
		sprintf(foo, "$%d", t->attribute.ival);
		spew(fmt, foo, opt);
		return;
	}

	scope_searchall(top, t->attribute.sval->name);
	if (t->attribute.sval->depth != 0) {
		spew("\tmovq\t(%%rbp), %%rcx\n");
		for (i = t->attribute.sval->depth - 1; i != 0; i--) {
			spew("\tmovq\t(%%rcx), %%rcx\n");
		}
		sprintf(foo,"%d(%%rcx)", t->attribute.sval->offset);
		spew(fmt, foo, opt);
	}
	else {
		sprintf(foo,"%d(%%rbp)", t->attribute.sval->offset);
		spew(fmt, foo, opt);
	}
}

int gen_write(char * name, tree_t * t)
{
	if (!t || strcmp("write", name)) {
		return 0;
	}
	tree_print(t);
	if (t->type != COMMA) {
		spew("\tmovq\t$0, %%rax\n");
		if (t->type == INUM)
			spew("\tmovq\t$%d, %%rsi\n", t->attribute.ival);
		else {
			spew("\tmovq\t%d(%%rbp), %%rsi\n", t->attribute.sval->offset);
		}
		spew("\tmovq\t$.LC0, %%rdi\n");
		spew("\tcall\tprintf\n");
		return 1;
	}

	spew("\tmovq\t$0, %%rax\n");
	if (t->right->type == INUM)
		spew("\tmovq\t$%d, %%rsi\n", t->right->attribute.ival);
	else {
		spew_id("\tmovq\t%s, %%rsi\n", t->right, NULL);
	}
	spew("\tmovq\t$.LC0, %%rdi\n");
	spew("\tcall\tprintf\n");
	gen_write("write", t->left);

	return 1;
}

int gen_read(char * name, tree_t * t)
{
	if (!t || strcmp("read", name)) {
		return 0;
	}

	spew_id("\tleaq\t%s, %%rsi\n", t->right, NULL);
	spew("\tmovq\t$.LC1, %%rdi\n");
	spew("\tmovq\t$0, %%rax\n");
	spew("\tcall\tscanf\n");

	// TODO

	return 1;
}

static int gen_addop(tree_t * t, reg_t * l, reg_t * r)
{
	if (l == NULL) {
		if (t->attribute.opval == PLUS) {
			spew_id("\taddq\t%s,%s\n",t->right, registers[r->num]);
			return 0;
			if (t->right->type == INUM) {
				spew("\taddq\t$%d, %s\n", t->right->attribute.ival, registers[r->num]);
			}
			else {
				spew_id("\taddq\t%s, %s\n", t->right, registers[r->num]);
			}
		}
		else if (t->attribute.opval == MINUS) {
			if (t->right->type == INUM)
				spew("\tsubq\t$%d, %s\n", t->right->attribute.ival, registers[r->num]);
			else
				spew_id("\tsubq\t$%d, %s\n", t->right, registers[r->num]);
		}
		return 0;
	}
	assert(r);
	fprintf(stderr,"yep,getting here\n");
	spew("\taddq\t%s, %s\n", registers[l->num], registers[r->num]);

	return 0;
}

static int gen_mulop(tree_t * t, reg_t * l, reg_t * r)
{
	if (l == NULL) {
		spew("\timulq\t$%d, %s\n", t->right->attribute.ival, registers[r->num]);
		return 0;
	}
	assert(r);

	spew("\timulq\t%s, %s\n", registers[l->num], registers[r->num]);

	return 0;
}

static int gen_op(tree_t * t, reg_t * l, reg_t * r)
{
	switch (t->type) {
		case ADDOP: gen_addop(t,l,r); break;
		case MULOP: gen_mulop(t,l,r); break;
		default: fprintf(stderr, "BALRGH\n"); break;
	}

	return 0;
}


static int gen_go(tree_t * t)
{
	reg_t * r;

	if (!t->left && !t->left && t->rank == 0) {
		fprintf(stderr, "Well this is embarrassing...\n");
		return 0;
	}

	if (t->type == INUM) {
		spew("\tmovq\t$%d,%s\n",t->attribute.ival,registers[st.top->num]);
		return 0;
	}

	GENSWITCH(t) {
		case 0:
			// MOV to top
			printf("case 0\n");
			printf("MOV %d, %s\n", t->attribute.ival, registers[st.top->num]);
			spew_id("\tmovq\t%s, %s\n", t, registers[st.top->num]);
			break;
		case 1:
			printf("case 1\n");
			gen_go(t->left);
			gen_op(t, NULL, st.top);
			printf("OP %d, %s\n", t->right->attribute.ival, registers[st.top->num]);
			break;
		case 2:
			printf("case 2\n");
			reg_swap();
			gen_go(t->right);
			r = reg_pop();
			gen_go(t->left);
			printf("OP %s, %s\n", registers[r->num], registers[st.top->num]);
			gen_op(t, r, st.top);
			reg_push(r);
			reg_swap();
		case 3:
			printf("case 3\n");
			gen_go(t->left);
			r = reg_pop();
			gen_go(t->right);
			printf("OP %s, %s\n", registers[st.top->num], registers[r->num]);
			gen_op(t, st.top, r);
			reg_push(r);
			break;

		default: fprintf(stderr, "Wtf? Case 4'd\n"); assert(0); break;
	}

	return 0;
}

int gencode(tree_t * t)
{
	if (!t) return -1;
	tree_print(t);

	printf("gencoding\n");

	if (t->type == ASNOP) {
		if (t->right->type == INUM) {
			spew("\tmovq\t$%d, %d(%%rbp)\n", t->right->attribute.ival, t->left->attribute.sval->offset);
			goto end;
		}
		else if (t->right->type == ID) {
			// This might cause issues with scoping
			spew("\tmovq\t%d(%%rbp), %%rdx\n", t->right->attribute.sval->offset);
			spew("\tmovq\t%%rdx, %d(%%rbp)\n", t->left->attribute.sval->offset);
			goto end;
		}
		gen_rankify(t->right);
		fprintf(stderr, "ASNOP to a %d\n", t->right->attribute.opval);
		reg_init();
		gen_rankify(t->right);
		gen_go(t->right);
		spew("\tmovq\t%s, %d(%%rbp)\n", registers[st.top->num], t->left->attribute.sval->offset);
		reg_deinit();
	}
	else if (t->type == PROCEDURE_CALL) {
		gencode(t->right);
		spew("\tcall\t%s\n", t->left->attribute.sval->name);

	}
	else if (t->type == COMMA) {
		reg_init();
		gen_rankify(t->right);
		gen_go(t->right);
		spew("\tpushq\t%s\n", registers[st.top->num]);
		reg_deinit();
		gencode(t->left);
	}
	else if (t->type == RELOP) {
		fprintf(stderr, "YEEEEP\n");
		reg_init();
		gen_rankify(t->left);
		gen_go(t->left);
		spew("\tpushq\t%s\n",registers[st.top->num]);
		reg_deinit();
		reg_init();
		gen_rankify(t->right);
		gen_go(t->right);
		spew("\tmovq\t%s,%s\n","(%rsp)", "%rdx");
		spew("\tpopq\t%s\n",registers[st.top->num]);
		spew("\tcmpq\t%s,%s\n",registers[st.top->num], "%rdx");
		reg_deinit();
		gen_jmp(t);
		gen_label();
	}
	else {
		assert(0);
	}

end:


	return 0;
}
