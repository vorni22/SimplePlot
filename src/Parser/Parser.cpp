#include "Parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace {
struct OpStackItem {
	bool is_func;
	Parser::OperatorType op;
	Parser::FunctionType func;
	char paren;
	bool is_func_paren;
};

struct OpStack {
	OpStackItem *data;
	uint16_t size;
	uint16_t capacity;
};

struct FuncArgFrame {
	Parser::FunctionType func;
	uint8_t comma_count;
	uint8_t depth;
};

struct FuncArgStack {
	FuncArgFrame *data;
	uint16_t size;
	uint16_t capacity;
};

bool stack_init(OpStack &stack, uint16_t capacity) {
	stack.data = new OpStackItem[capacity];
	if (!stack.data) {
		return false;
	}
	stack.size = 0;
	stack.capacity = capacity;
	return true;
}

void stack_free(OpStack &stack) {
	delete[] stack.data;
	stack.data = 0;
	stack.size = 0;
	stack.capacity = 0;
}

bool stack_push(OpStack &stack, const OpStackItem &item) {
	if (stack.size >= stack.capacity) {
		return false;
	}
	stack.data[stack.size++] = item;
	return true;
}

bool stack_pop(OpStack &stack, OpStackItem &item) {
	if (stack.size == 0) {
		return false;
	}
	item = stack.data[--stack.size];
	return true;
}

bool stack_top(const OpStack &stack, OpStackItem &item) {
	if (stack.size == 0) {
		return false;
	}
	item = stack.data[stack.size - 1];
	return true;
}

bool stack_empty(const OpStack &stack) {
	return stack.size == 0;
}

bool func_stack_init(FuncArgStack &stack, uint16_t capacity) {
	stack.data = new FuncArgFrame[capacity];
	if (!stack.data) {
		return false;
	}
	stack.size = 0;
	stack.capacity = capacity;
	return true;
}

void func_stack_free(FuncArgStack &stack) {
	delete[] stack.data;
	stack.data = 0;
	stack.size = 0;
	stack.capacity = 0;
}

bool func_stack_push(FuncArgStack &stack, const FuncArgFrame &item) {
	if (stack.size >= stack.capacity) {
		return false;
	}
	stack.data[stack.size++] = item;
	return true;
}

bool func_stack_pop(FuncArgStack &stack, FuncArgFrame &item) {
	if (stack.size == 0) {
		return false;
	}
	item = stack.data[--stack.size];
	return true;
}

FuncArgFrame *func_stack_top(FuncArgStack &stack) {
	if (stack.size == 0) {
		return 0;
	}
	return &stack.data[stack.size - 1];
}

bool func_stack_empty(const FuncArgStack &stack) {
	return stack.size == 0;
}

bool match_word(const char *s, uint16_t &i, const char *word) {
	uint16_t len = (uint16_t)strlen(word);
	if (strncmp(s + i, word, len) != 0) {
		return false;
	}
	i = (uint16_t)(i + len);
	return true;
}

bool parse_number_q16(const char *s, uint16_t &i, int32_t &out_q16) {
	uint16_t pos = i;
	bool has_digit = false;
	uint32_t int_part = 0;

	while (s[pos] >= '0' && s[pos] <= '9') {
		has_digit = true;
		int_part = (uint32_t)(int_part * 10u + (uint32_t)(s[pos] - '0'));
		++pos;
	}

	uint32_t frac_part = 0;
	uint32_t frac_div = 1;

	if (s[pos] == '.') {
		++pos;
		while (s[pos] >= '0' && s[pos] <= '9') {
			has_digit = true;
			if (frac_div < 1000000000u) {
				frac_part = (uint32_t)(frac_part * 10u + (uint32_t)(s[pos] - '0'));
				frac_div *= 10u;
			}
			++pos;
		}
	}

	if (!has_digit) {
		return false;
	}

	int32_t q16 = (int32_t)(int_part << 16);
	if (frac_div > 1u) {
		uint32_t frac_q16 = (uint32_t)(((uint64_t)frac_part << 16) / frac_div);
		q16 = (int32_t)(q16 + (int32_t)frac_q16);
	}

	out_q16 = q16;
	i = pos;
	return true;
}
}

void Parser::init_buffer(TokenBuffer &buffer, uint16_t capacity) {
	buffer.data = new Token[capacity];
	buffer.size = 0;
	buffer.capacity = capacity;
}

void Parser::free_buffer(TokenBuffer &buffer) {
	delete[] buffer.data;
	buffer.data = 0;
	buffer.size = 0;
	buffer.capacity = 0;
}

bool Parser::push_token(TokenBuffer &buffer, const Token &token) {
	if (buffer.size >= buffer.capacity) {
		return false;
	}
	buffer.data[buffer.size++] = token;
	return true;
}

void Parser::print_token_buffer(const TokenBuffer &buffer) {
	for (uint16_t i = 0; i < buffer.size; ++i) {
		const Token &t = buffer.data[i];
		switch (t.type) {
			case TokenType::Number: {
				int32_t v = t.number_q16;
				if (v < 0) {
					v = -v;
					printf("-");
				}
				int32_t whole = (int32_t)(v >> 16);
				uint32_t frac = (uint32_t)(v & 0xFFFFu);
				uint32_t frac_dec = (uint32_t)(((uint64_t)frac * 10000u) >> 16);
				printf("%ld.%04lu ", (long)whole, (unsigned long)frac_dec);
				break;
			}
			case TokenType::VariableX:
				printf("x ");
				break;
			case TokenType::VariableY:
				printf("y ");
				break;
			case TokenType::Operator:
				switch (t.op) {
					case OperatorType::Add:
						printf("+ ");
						break;
					case OperatorType::Sub:
						printf("- ");
						break;
					case OperatorType::Mul:
						printf("* ");
						break;
					case OperatorType::Div:
						printf("/ ");
						break;
					case OperatorType::Neg:
						printf("neg ");
						break;
				}
				break;
			case TokenType::Function:
				switch (t.func) {
					case FunctionType::Sin:
						printf("sin ");
						break;
					case FunctionType::Cos:
						printf("cos ");
						break;
					case FunctionType::Exp:
						printf("exp ");
						break;
					case FunctionType::Log:
						printf("log ");
						break;
					case FunctionType::Log1:
						printf("log1 ");
						break;
					case FunctionType::Log2:
						printf("log2 ");
						break;
					case FunctionType::Floor:
						printf("floor ");
						break;
					case FunctionType::Ceil:
						printf("ceil ");
						break;
					case FunctionType::Pow:
						printf("pow ");
						break;
					case FunctionType::Pow1:
						printf("pow1 ");
						break;
					case FunctionType::Pow2:
						printf("pow2 ");
						break;
				}
				break;
		}
	}
	printf("\n");
}

bool Parser::parse(const char *expr, TokenBuffer &out_rpn) {
	out_rpn.size = 0;
	if (expr == 0 || out_rpn.data == 0) {
		return false;
	}

	OpStack op_stack;
	if (!stack_init(op_stack, out_rpn.capacity)) {
		return false;
	}

	FuncArgStack func_stack;
	if (!func_stack_init(func_stack, out_rpn.capacity)) {
		stack_free(op_stack);
		return false;
	}

	uint16_t i = 0;
	bool expect_value = true;
	bool pending_func = false;
	FunctionType pending_func_type = FunctionType::Sin;

	while (expr[i] != '\0') {
		char c = expr[i];
		if (is_space(c)) {
			++i;
			continue;
		}

		if (pending_func && c != '(') {
			stack_free(op_stack);
			func_stack_free(func_stack);
			return false;
		}

		if (is_digit(c) || (c == '.' && is_digit(expr[i + 1]))) {
			int32_t value_q16 = 0;
			if (!parse_number_q16(expr, i, value_q16)) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}

			Token t;
			t.type = TokenType::Number;
			t.number_q16 = value_q16;
			if (!push_token(out_rpn, t)) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}
			expect_value = false;
			continue;
		}

		if (is_alpha(c)) {
			if (c == 'x') {
				Token t;
				t.type = TokenType::VariableX;
				if (!push_token(out_rpn, t)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				++i;
				expect_value = false;
				continue;
			}
			if (c == 'y') {
				Token t;
				t.type = TokenType::VariableY;
				if (!push_token(out_rpn, t)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				++i;
				expect_value = false;
				continue;
			}

			OpStackItem func;
			func.is_func = true;
			func.paren = 0;
			func.is_func_paren = false;

			if (match_word(expr, i, "sin")) {
				func.func = FunctionType::Sin;
			} else if (match_word(expr, i, "cos")) {
				func.func = FunctionType::Cos;
			} else if (match_word(expr, i, "exp")) {
				func.func = FunctionType::Exp;
			} else if (match_word(expr, i, "log")) {
				func.func = FunctionType::Log;
			} else if (match_word(expr, i, "floor")) {
				func.func = FunctionType::Floor;
			} else if (match_word(expr, i, "ceil")) {
				func.func = FunctionType::Ceil;
			} else if (match_word(expr, i, "pow")) {
				func.func = FunctionType::Pow;
			} else {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}

			if (!stack_push(op_stack, func)) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}
			pending_func = true;
			pending_func_type = func.func;
			expect_value = true;
			continue;
		}

		if (c == '(') {
			OpStackItem p;
			p.is_func = false;
			p.paren = '(';
			p.is_func_paren = pending_func;
			if (!stack_push(op_stack, p)) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}
			if (pending_func) {
				FuncArgFrame frame;
				frame.func = pending_func_type;
				frame.comma_count = 0;
				frame.depth = 0;
				if (!func_stack_push(func_stack, frame)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				pending_func = false;
			} else {
				FuncArgFrame *frame = func_stack_top(func_stack);
				if (frame) {
					++frame->depth;
				}
			}
			++i;
			expect_value = true;
			continue;
		}

		if (c == ')') {
			bool found_left = false;
			OpStackItem left_paren;
			while (!stack_empty(op_stack)) {
				OpStackItem top;
				stack_pop(op_stack, top);

				if (!top.is_func && top.paren == '(') {
					found_left = true;
					left_paren = top;
					break;
				}

				Token t;
				if (top.is_func) {
					t.type = TokenType::Function;
					t.func = top.func;
				} else {
					t.type = TokenType::Operator;
					t.op = top.op;
				}
				if (!push_token(out_rpn, t)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
			}

			if (!found_left) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}

			if (!left_paren.is_func_paren) {
				FuncArgFrame *frame = func_stack_top(func_stack);
				if (frame) {
					if (frame->depth == 0) {
						stack_free(op_stack);
						func_stack_free(func_stack);
						return false;
					}
					--frame->depth;
				}
			}

			OpStackItem top;
			if (stack_top(op_stack, top) && top.is_func) {
				if (expect_value) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				FuncArgFrame frame;
				if (!func_stack_pop(func_stack, frame)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				uint8_t arg_count = (uint8_t)(frame.comma_count + 1);
				if (frame.func == FunctionType::Pow || frame.func == FunctionType::Log) {
					if (arg_count != 1 && arg_count != 2) {
						stack_free(op_stack);
						func_stack_free(func_stack);
						return false;
					}
				} else if (arg_count != 1) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}

				Token t;
				t.type = TokenType::Function;
				if (frame.func == FunctionType::Pow) {
					t.func = (arg_count == 1) ? FunctionType::Pow1 : FunctionType::Pow2;
				} else if (frame.func == FunctionType::Log) {
					t.func = (arg_count == 1) ? FunctionType::Log1 : FunctionType::Log2;
				} else {
					t.func = top.func;
				}
				if (!push_token(out_rpn, t)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
				OpStackItem dummy;
				stack_pop(op_stack, dummy);
			}

			++i;
			expect_value = false;
			continue;
		}

		if (c == ',') {
			if (expect_value) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}

			FuncArgFrame *frame = func_stack_top(func_stack);
			if (!frame || frame->depth != 0) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}
			++frame->comma_count;

			bool found_left = false;
			while (!stack_empty(op_stack)) {
				OpStackItem top;
				stack_top(op_stack, top);
				if (!top.is_func && top.paren == '(') {
					found_left = true;
					break;
				}
				stack_pop(op_stack, top);
				Token t;
				if (top.is_func) {
					t.type = TokenType::Function;
					t.func = top.func;
				} else {
					t.type = TokenType::Operator;
					t.op = top.op;
				}
				if (!push_token(out_rpn, t)) {
					stack_free(op_stack);
					func_stack_free(func_stack);
					return false;
				}
			}

			if (!found_left) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}

			++i;
			expect_value = true;
			continue;
		}

		if (c == '+' || c == '-' || c == '*' || c == '/') {
			OperatorType op;
			if (c == '-' && expect_value) {
				op = OperatorType::Neg;
			} else if (c == '+') {
				op = OperatorType::Add;
			} else if (c == '-') {
				op = OperatorType::Sub;
			} else if (c == '*') {
				op = OperatorType::Mul;
			} else {
				op = OperatorType::Div;
			}

			while (!stack_empty(op_stack)) {
				OpStackItem top;
				stack_top(op_stack, top);
				if (top.is_func || top.paren == '(') {
					break;
				}

				uint8_t prec_top = precedence(top.op);
				uint8_t prec_op = precedence(op);
				if (prec_top > prec_op ||
					(prec_top == prec_op && !is_right_assoc(op))) {
					Token t;
					t.type = TokenType::Operator;
					t.op = top.op;
					if (!push_token(out_rpn, t)) {
						stack_free(op_stack);
						func_stack_free(func_stack);
						return false;
					}
					OpStackItem dummy;
					stack_pop(op_stack, dummy);
					continue;
				}
				break;
			}

			OpStackItem item;
			item.is_func = false;
			item.op = op;
			item.paren = 0;
			item.is_func_paren = false;
			if (!stack_push(op_stack, item)) {
				stack_free(op_stack);
				func_stack_free(func_stack);
				return false;
			}
			++i;
			expect_value = true;
			continue;
		}

		stack_free(op_stack);
		func_stack_free(func_stack);
		return false;
	}

	if (expect_value) {
		stack_free(op_stack);
		func_stack_free(func_stack);
		return false;
	}

	while (!stack_empty(op_stack)) {
		OpStackItem top;
		stack_pop(op_stack, top);
		if (!top.is_func && top.paren == '(') {
			stack_free(op_stack);
			func_stack_free(func_stack);
			return false;
		}

		Token t;
		if (top.is_func) {
			t.type = TokenType::Function;
			t.func = top.func;
		} else {
			t.type = TokenType::Operator;
			t.op = top.op;
		}
		if (!push_token(out_rpn, t)) {
			stack_free(op_stack);
			func_stack_free(func_stack);
			return false;
		}
	}

	stack_free(op_stack);
	func_stack_free(func_stack);
	return true;
}

bool Parser::is_space(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool Parser::is_digit(char c) {
	return c >= '0' && c <= '9';
}

bool Parser::is_alpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

uint8_t Parser::precedence(OperatorType op) {
	switch (op) {
		case OperatorType::Neg:
			return 3;
		case OperatorType::Mul:
		case OperatorType::Div:
			return 2;
		case OperatorType::Add:
		case OperatorType::Sub:
		default:
			return 1;
	}
}

bool Parser::is_right_assoc(OperatorType op) {
	return op == OperatorType::Neg;
}
