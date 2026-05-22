#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

class Parser {
public:
	enum class TokenType : uint8_t {
		Number,
		VariableX,
		VariableY,
		Operator,
		Function
	};

	enum class OperatorType : uint8_t {
		Add,
		Sub,
		Mul,
		Div,
		Neg
	};

	enum class FunctionType : uint8_t {
		Sin,
		Cos,
		Exp,
		Log,
		Log1,
		Log2,
		Floor,
		Ceil,
		Pow,
		Pow1,
		Pow2,
	};

	struct Token {
		TokenType type;
		int32_t number_q16;
		OperatorType op;
		FunctionType func;
	};

	struct TokenBuffer {
		Token *data;
		uint16_t size;
		uint16_t capacity;
	};

	static void init_buffer(TokenBuffer &buffer, uint16_t capacity);
	static void free_buffer(TokenBuffer &buffer);
	static bool parse(const char *expr, TokenBuffer &out_rpn);
	static void print_token_buffer(const TokenBuffer &buffer);

private:
	static bool push_token(TokenBuffer &buffer, const Token &token);
	static bool is_space(char c);
	static bool is_digit(char c);
	static bool is_alpha(char c);
	static uint8_t precedence(OperatorType op);
	static bool is_right_assoc(OperatorType op);
};

#endif