#ifndef XIHASH_TOKENIZER_H
#define XIHASH_TOKENIZER_H

#include <string_view>
#include <vector>

#include "Tokenize/TokenizeGenerall.h"

// Tokenizer converts XiHash source text into a sequence of location-aware tokens.
class Tokenizer {
public:
    static std::vector<Token> tokenize(std::string_view source);
};

#endif // XIHASH_TOKENIZER_H
