#include "PlotFunction.h"
#include "Geometry.h"

#include <limits.h>

void PlotFunction::set_tokens(const Parser::TokenBuffer *tokens) {
    tokens_ = tokens;
}

static int32_t fx_floor_q16(int32_t v) {
    return (int32_t)(v & 0xFFFF0000);
}

static int32_t fx_ceil_q16(int32_t v) {
    if ((v & 0xFFFF) == 0) {
        return v;
    }
    if (v >= 0) {
        return (int32_t)((v & 0xFFFF0000) + (1LL << 16));
    }
    return (int32_t)(v & 0xFFFF0000);
}

static int32_t log_base_q16(int32_t base_q16, int32_t value_q16) {
    int32_t ln_base = fx_ln_q16(base_q16);
    int32_t ln_value = fx_ln_q16(value_q16);
    if (ln_base == INT32_MIN) {
        return INT32_MIN;
    }
    return fx_div(ln_value, ln_base);
}

int32_t PlotFunction::get_value(int32_t x_q16, int32_t y_q16)
{
    if (!tokens_ || tokens_->size == 0 || !tokens_->data) {
        return 0;
    }

    int32_t stack[64];
    uint8_t sp = 0;

    for (uint16_t i = 0; i < tokens_->size; ++i) {
        const Parser::Token &t = tokens_->data[i];
        switch (t.type) {
            case Parser::TokenType::Number:
                if (sp >= 64) return 0;
                stack[sp++] = t.number_q16;
                break;
            case Parser::TokenType::VariableX:
                if (sp >= 64) return 0;
                stack[sp++] = x_q16;
                break;
            case Parser::TokenType::VariableY:
                if (sp >= 64) return 0;
                stack[sp++] = y_q16;
                break;
            case Parser::TokenType::Operator: {
                if (t.op == Parser::OperatorType::Neg) {
                    if (sp < 1) return 0;
                    stack[sp - 1] = -stack[sp - 1];
                    break;
                }
                if (sp < 2) return 0;
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                int32_t r = 0;
                switch (t.op) {
                    case Parser::OperatorType::Add:
                        r = a + b;
                        break;
                    case Parser::OperatorType::Sub:
                        r = a - b;
                        break;
                    case Parser::OperatorType::Mul:
                        r = fx_mul(a, b);
                        break;
                    case Parser::OperatorType::Div:
                        r = fx_div(a, b);
                        break;
                    case Parser::OperatorType::Neg:
                    default:
                        r = 0;
                        break;
                }
                stack[sp++] = r;
                break;
            }
            case Parser::TokenType::Function: {
                switch (t.func) {
                    case Parser::FunctionType::Sin: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = sin_rad_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Cos: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = cos_rad_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Exp: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = fx_exp_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Log1: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = fx_ln_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Log2: {
                        if (sp < 2) return 0;
                        int32_t value = stack[--sp];
                        int32_t base = stack[--sp];
                        stack[sp++] = log_base_q16(base, value);
                        break;
                    }
                    case Parser::FunctionType::Floor: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = fx_floor_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Ceil: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = fx_ceil_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Pow1: {
                        if (sp < 1) return 0;
                        stack[sp - 1] = fx_exp_q16(stack[sp - 1]);
                        break;
                    }
                    case Parser::FunctionType::Pow2: {
                        if (sp < 2) return 0;
                        int32_t exp = stack[--sp];
                        int32_t base = stack[--sp];
                        stack[sp++] = fx_pow_q16(base, exp);
                        break;
                    }
                    case Parser::FunctionType::Log:
                    case Parser::FunctionType::Pow:
                    default:
                        return 0;
                }
                break;
            }
            default:
                return 0;
        }
    }

    if (sp != 1) {
        return 0;
    }
    return stack[0];
}
