#ifndef PLOT_FUNCTION_H
#define PLOT_FUNCTION_H

#include <inttypes.h>

#include "Parser/Parser.h"

class PlotFunction {
public:
    void set_tokens(const Parser::TokenBuffer *tokens);
    int32_t get_value(int32_t x_q16, int32_t y_q16);

private:
    const Parser::TokenBuffer *tokens_;
};


#endif