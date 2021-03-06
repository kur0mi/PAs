#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

bool check_negtive_prefixx(int t);
bool check_inner_prefixx(int t);

enum {
	TK_NOTYPE = 256, TK_EQ, TK_NEQ,

	/* TODO: Add more token types */
	TK_DECIMAL, TK_HEXADECIMAL,
	TK_REGNAME,
	TK_COMMA,
	TK_OPEN_PAREN, TK_CLOSE_PAREN,
	TK_LOGIC_AND, TK_LOGIC_OR,
	TK_LOGIC_NOT,
	TK_NEGTIVE, TK_INNER
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{
	"^( +)", TK_NOTYPE},	// spaces
	{
	"^(==)", TK_EQ},	// equal
/*
    {
	"^(<)", '<'},	// less
    {
	"^(>)", '>'},	// more
*/
	{
	"^(!=)", TK_NEQ},	// not equal
	{
	"^(\\+)", '+'},		// plus
	{
	"^(-)", '-'},		// sub or neg
	{
	"^(\\*)", '*'},		// multi or inner
	{
	"^(/)", '/'},		// div
	{
	"^(0x[0-9a-fA-F]+)", TK_HEXADECIMAL},	// hexadecimal numbers
	{
	"^(0|[1-9][0-9]*)", TK_DECIMAL},	// decimal numbers
	{
	"^([$%]?(e?(ax|bx|cx|dx|sp|bp|si|di|ip)|[abcd][lh]|IF|SF|OF|CF|ZF))", TK_REGNAME},	// reg name
	{
	"^(,)", TK_COMMA},	// comma
	{
	"^(\\()", TK_OPEN_PAREN},	// open paren
	{
	"^(\\))", TK_CLOSE_PAREN},	// close paren
	{
	"^(&&)", TK_LOGIC_AND},	// logic and
	{
	"^[|]{2}", TK_LOGIC_OR},	//logic or
	{
	"^(!)", TK_LOGIC_NOT},	// logic not
};

// C has not (?!pattern), em fxxk...

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
	int i;
	char error_msg[128];
	int ret;

	for (i = 0; i < NR_REGEX; i++) {
		// 忽略大小写
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED | REG_ICASE);
		if (ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e)
{
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while (e[position] != '\0') {
		/* Try all rules one by one. */
		for (i = 0; i < NR_REGEX; i++) {
			// if match success
			if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0) {
				if (nr_token >= 31) {
					Log("too many tokens");
					return false;
				}
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				//Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */
				switch (rules[i].token_type) {
				case TK_DECIMAL:
				case TK_HEXADECIMAL:
				case TK_REGNAME:
					if (substr_len > 31) {
						Log("token descrip to long");
						return false;
					}
					strncpy(tokens[nr_token].str, substr_start, substr_len);
					tokens[nr_token].str[substr_len] = '\0';
					tokens[nr_token].type = rules[i].token_type;
					break;
				case TK_NOTYPE:
					continue;
				case '-':
					if (nr_token == 0 || check_negtive_prefixx(nr_token - 1)) {
						tokens[nr_token].type = TK_NEGTIVE;
						//Log("negtive");
					} else {
						tokens[nr_token].type = rules[i].token_type;
						//Log("sub");
					}
					break;
				case '*':
					if (nr_token == 0 || check_inner_prefixx(nr_token - 1)) {
						tokens[nr_token].type = TK_INNER;
						break;
					} else {
						tokens[nr_token].type = rules[i].token_type;;
						break;
					}
				default:
					tokens[nr_token].type = rules[i].token_type;
				}

				nr_token++;
				break;
			}
		}

		if (i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true;
}

// check open_paren & close_paren
bool check_parentheses(int p, int q)
{
	if (tokens[p].type == TK_OPEN_PAREN && tokens[q].type == TK_CLOSE_PAREN) {
		int nO = 0;
		int nC = 0;
		p++;
		while (p < q) {
			if (tokens[p].type == TK_OPEN_PAREN)
				nO++;
			else if (tokens[p].type == TK_CLOSE_PAREN) {
				nC++;
				if (nC > nO)
					return false;
			}
			p++;
		}
		if (nC == nO)
			return true;
		else
			return false;
	} else
		return false;
}

bool check_inner_prefixx(int t)
{
	int type = tokens[t].type;
	if (type == '+' || type == '-' || type == '*' || type == TK_INNER || type == '/' || type == TK_NEGTIVE || type == TK_OPEN_PAREN || type == TK_COMMA || type == TK_EQ)
		return true;
	else
		return false;
}

bool check_negtive_prefixx(int t)
{
	int type = tokens[t].type;
	if (type == '+' || type == '-' || type == '*' || type == TK_INNER || type == '/' || type == TK_NEGTIVE || type == TK_OPEN_PAREN || type == TK_COMMA || type == TK_EQ)
		return true;
	else
		return false;
}

// check possible domi
bool check_domi_operate(int t)
{
	int type = tokens[t].type;
	if (type == '+' || type == '-' || type == '*' || type == TK_INNER || type == '/' || type == TK_NEGTIVE || type == TK_COMMA || type == TK_EQ || type == TK_NEQ || type == TK_LOGIC_AND || type == TK_LOGIC_OR || type == TK_LOGIC_NOT)
		return true;
	else
		return false;
}

int get_dominant(int p, int q)
{
	int domi = -1;
	int min_level = 15;
	int level;
	int cur;
	for (cur = p; cur <= q; cur++) {
		// jump over bracks
		if (tokens[cur].type == TK_OPEN_PAREN) {
			cur++;
			while (tokens[cur].type != TK_CLOSE_PAREN)
				cur++;
			cur++;
			if (cur > q)
				break;
		}
		// jump non_diminant
		if (!check_domi_operate(cur))
			continue;

		switch (tokens[cur].type) {
		case TK_NEGTIVE:
		case TK_INNER:
		case TK_LOGIC_NOT:
			level = 9;
			break;
		case '*':
		case '/':
			level = 8;
			break;
		case '+':
		case '-':
			level = 7;
			break;
		case TK_EQ:
		case TK_NEQ:
			level = 6;
			break;
		case TK_LOGIC_AND:
			level = 5;
			break;
		case TK_LOGIC_OR:
			level = 4;
			break;
		case TK_COMMA:
			level = 1;
			break;
		default:
			assert(0);
		}

		if (level < min_level) {
			min_level = level;
			domi = cur;
		}
	}

	Assert(domi != -1, "cannot find dominant");
	return domi;
}

uint32_t eval(int p, int q)
{
	//Log("eval %d - %d", p, q);
	if (p > q) {
		panic("Bad expression");
	} else if (p == q) {
		uint32_t n = 0;
		if (tokens[p].type == TK_DECIMAL) {
			sscanf(tokens[p].str, "%d", &n);
			return n;
		} else if (tokens[p].type == TK_HEXADECIMAL) {
			sscanf(tokens[p].str + 2, "%x", &n);
			return n;
		} else if (tokens[p].type == TK_REGNAME) {
			// eax, ebx, ebx, ecx, esp, ebp, esi, edi
			// ax, dx, bx, cx, sp, bp, si, di
			// al, cl, dl, bl, ah, ch, dh, bh
			// eip, ip
			// zf, cf, sf, of, if

			if (strstr(tokens[p].str, "eax") != NULL)
				return reg_val(R_EAX, 4);
			else if (strstr(tokens[p].str, "ecx") != NULL)
				return reg_val(R_ECX, 4);
			else if (strstr(tokens[p].str, "edx") != NULL)
				return reg_val(R_EDX, 4);
			else if (strstr(tokens[p].str, "ebx") != NULL)
				return reg_val(R_EBX, 4);
			else if (strstr(tokens[p].str, "esp") != NULL)
				return reg_val(R_ESP, 4);
			else if (strstr(tokens[p].str, "ebp") != NULL)
				return reg_val(R_EBP, 4);
			else if (strstr(tokens[p].str, "esi") != NULL)
				return reg_val(R_ESI, 4);
			else if (strstr(tokens[p].str, "edi") != NULL)
				return reg_val(R_EDI, 4);

			else if (strstr(tokens[p].str, "ax") != NULL)
				return reg_val(R_EAX, 2);
			else if (strstr(tokens[p].str, "cx") != NULL)
				return reg_val(R_ECX, 2);
			else if (strstr(tokens[p].str, "dx") != NULL)
				return reg_val(R_EDX, 2);
			else if (strstr(tokens[p].str, "bx") != NULL)
				return reg_val(R_EBX, 2);
			else if (strstr(tokens[p].str, "sp") != NULL)
				return reg_val(R_ESP, 2);
			else if (strstr(tokens[p].str, "bp") != NULL)
				return reg_val(R_EBP, 2);
			else if (strstr(tokens[p].str, "si") != NULL)
				return reg_val(R_ESI, 2);
			else if (strstr(tokens[p].str, "di") != NULL)
				return reg_val(R_EDI, 2);

			else if (strstr(tokens[p].str, "al") != NULL)
				return reg_val(R_EAX, 1);
			else if (strstr(tokens[p].str, "cl") != NULL)
				return reg_val(R_ECX, 1);
			else if (strstr(tokens[p].str, "dl") != NULL)
				return reg_val(R_EDX, 1);
			else if (strstr(tokens[p].str, "bl") != NULL)
				return reg_val(R_EBX, 1);
			else if (strstr(tokens[p].str, "ah") != NULL)
				return reg_val(R_ESP, 1);
			else if (strstr(tokens[p].str, "ch") != NULL)
				return reg_val(R_EBP, 1);
			else if (strstr(tokens[p].str, "dh") != NULL)
				return reg_val(R_ESI, 1);
			else if (strstr(tokens[p].str, "bh") != NULL)
				return reg_val(R_EDI, 1);

			else if (strstr(tokens[p].str, "eip") != NULL)
				return cpu.eip;
			else if (strstr(tokens[p].str, "ip") != NULL)
				return cpu.eip & 0xffff;

			else if (strstr(tokens[p].str, "of") != NULL)
				return cpu.eflags.OF;
			else if (strstr(tokens[p].str, "sf") != NULL)
				return cpu.eflags.SF;
			else if (strstr(tokens[p].str, "zf") != NULL)
				return cpu.eflags.ZF;
			else if (strstr(tokens[p].str, "cf") != NULL)
				return cpu.eflags.CF;
			else if (strstr(tokens[p].str, "if") != NULL)
				return cpu.eflags.IF;

			AlarmText("no such register: %s", tokens[p].str);
			return 0;
		} else {
			AlarmText("no such unit: %s", tokens[p].str);
			return 0;
		}
	} else {
		while (check_parentheses(p, q))
			return eval(p + 1, q - 1);

		int domi = get_dominant(p, q);
		if (tokens[domi].type == TK_NEGTIVE)
			return -1 * eval(domi + 1, q);
		if (tokens[domi].type == TK_INNER)
			return vaddr_read(eval(domi + 1, q), 4);
		if (tokens[domi].type == TK_LOGIC_NOT)
			return !(eval(domi + 1, q));

		int val1 = eval(p, domi - 1);
		int val2 = eval(domi + 1, q);
		switch (tokens[domi].type) {
		case '+':
			return val1 + val2;
		case '-':
			return val1 - val2;
		case '*':
			return val1 * val2;
		case '/':
			return val1 / val2;
		case TK_EQ:
			return val1 == val2;
		case TK_NEQ:
			return val1 != val2;
		case TK_LOGIC_AND:
			return val1 && val2;
		case TK_LOGIC_OR:
			return val1 || val2;
		case TK_COMMA:
			return val2;
		default:
			Log("未识别的符号");
			return 0;
		}
	}
}

uint32_t expr(char *e)
{
	if (!make_token(e)) {
		Log("make tokens failed");
		return 0;
	}

	/* TODO: Insert codes to evaluate the expression. */
	return eval(0, nr_token - 1);
}

#ifndef MY_RELEASE
// expr 函数测试
void expr_test()
{
#define N 22
	char es[][N] = { "3+2", "3-2", "3*2", "3/2",
		"3==3", "3==2", "3!=3", "3!=2",
		"1&&0", "1&&1", "1||1", "1||0",
		"!3", "!0", "32", "0", "0x00", "0xff",
		"(3+2)", "(-2*3)", "-(3+4)-(-2*3)", "3+2, 3*4",
	};
	int res[N] = { 5, 1, 6, 1,
		1, 0, 0, 1,
		0, 1, 1, 1,
		0, 1, 32, 0, 0, 255,
		5, -6, -1, 12,
	};
	int i;
	for (i = 0; i < N; i++) {
		Log("test: %s = %d", es[i], res[i]);
		Assert(res[i] == expr(es[i]), "expr_test fail.");
	}
}
#endif
