#ifndef __BITTY_ESCAPE_PARSER_HH__
#define __BITTY_ESCAPE_PARSER_HH__

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "util.hh"

namespace bitty {

class StringTok {};
class NumberTok {};
class ListOfNumbersTok {};

using Token = std::variant<char, std::string, u32, std::vector<u32>>;
using DfaToken = std::variant<char, StringTok, NumberTok, ListOfNumbersTok>;

struct EscapeCodeRule {
  std::vector<DfaToken> tokens;
  u16 rule_num;

  inline EscapeCodeRule(u16 rule, std::initializer_list<DfaToken> list)
      : tokens(list), rule_num(rule) {}
};

struct Transition {
  u16 number : 14;
  u16 exists : 1;
  u16 accept : 1;
};

class State {
  Transition transitions_char_[256] = {};
  Transition transition_num_{}, transition_str_{}, transition_num_list_{};

 public:
  inline bool HasStringTransition() const {
    return transition_str_.exists;
  }

  inline bool HasNumTransition() const {
    return transition_num_.exists;
  }

  inline bool HasNumListTransition() const {
    return transition_num_list_.exists;
  }

  inline bool HasCharTransition(uint8_t chr) const {
    return transitions_char_[chr].exists;
  }

  inline Transition NextByDfaToken(DfaToken tok) {
    return std::visit(
        Overloaded{[&](char ch) { return transitions_char_[(uint8_t)ch]; },
                   [&](StringTok) { return transition_str_; },
                   [&](NumberTok) { return transition_num_; },
                   [&](ListOfNumbersTok) { return transition_num_list_; }},
        tok);
  }

  inline void AddTransition(DfaToken tok, Transition transition) {
    std::visit(
        Overloaded{[&](char ch) mutable {
                     transitions_char_[(uint8_t)ch] = transition;
                   },
                   [&](StringTok) mutable { transition_str_ = transition; },
                   [&](NumberTok) mutable { transition_num_ = transition; },
                   [&](ListOfNumbersTok) mutable {
                     transition_num_list_ = transition;
                   }},
        tok);
  }

  inline Transition Next(const Token &tok) const {
    return std::visit(
        Overloaded{[&](char ch) { return transitions_char_[(uint8_t)ch]; },
                   [&](const std::string &) { return transition_str_; },
                   [&](u32) { return transition_num_; },
                   [&](const std::vector<u32> &) {
                     return transition_num_list_;
                   }},
        tok);
  }

#undef FIND_TRANSITION
};

using DfaState = u16;

class Dfa {
  std::vector<State> states_;

  DfaState AddState(State state);

 public:
  void AddRule(const EscapeCodeRule &rule);
  Dfa(const std::vector<EscapeCodeRule> &rules);
  Transition Eat(DfaState curr_state, Token token) const;

  inline bool HasStringTransition(DfaState curr_state) const {
    return states_.at(curr_state).HasStringTransition();
  }

  inline bool HasNumTransition(DfaState curr_state) const {
    return states_.at(curr_state).HasNumTransition();
  }

  inline bool HasNumListTransition(DfaState curr_state) const {
    return states_.at(curr_state).HasNumListTransition();
  }

  inline bool HasCharTransition(DfaState curr_state, uint8_t chr) const {
    return states_.at(curr_state).HasCharTransition(chr);
  }
};

struct EscapeParseResult {
  u16 rule_num;
  std::vector<Token> tokens;
};

constexpr int kEscapeRuleCount = 33;

enum class EatResult {
  kNone,
  kError,
  kAccept,
  kAcceptButLastByteIsExtra
};

class EscapeParser {
  static const Dfa dfa;

  struct NumParseState {
    u32 num;
    std::vector<u32> num_list;
  };

  struct StrParseState {
    std::string str;
    bool prev_was_escape;
  };

  bool result_ready_{false};
  EscapeParseResult result_{};

  DfaState dfa_state_{0};

  enum { kNone, kNumber, kListOfNums, kString } current_token_type_{kNone};

  std::variant<NumParseState, StrParseState> current_token_;

  EatResult PushToken(Token &&tok);

 public:  
  EatResult EatByte(char byte);
  EscapeParseResult Result();
};
}  // namespace bitty

#endif /* __BITTY_ESCAPE_PARSER_HH__ */