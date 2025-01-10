#include "escape_parser.hh"

#include <stdexcept>

namespace bitty {
constexpr static auto NumList = ListOfNumbersTok{};
constexpr static auto Num = NumberTok{};
constexpr static auto Str = StringTok{};

const Dfa EscapeParser::dfa{
    {// Formatting
     EscapeCodeRule(0, {'[', NumList, 'm'}), EscapeCodeRule(0, {'[', Num, 'm'}),
     EscapeCodeRule(0, {'[', 'm'}),
     // Cursor position manipulation
     EscapeCodeRule(1, {'[', Num, 'A'}), EscapeCodeRule(2, {'[', Num, 'B'}),
     EscapeCodeRule(3, {'[', Num, 'C'}), EscapeCodeRule(4, {'[', Num, 'D'}),
     EscapeCodeRule(5, {'[', Num, 'E'}), EscapeCodeRule(6, {'[', Num, 'F'}),
     EscapeCodeRule(7, {'[', Num, 'G'}), EscapeCodeRule(8, {'[', Num, 'H'}),
     EscapeCodeRule(1, {'[', 'A'}), EscapeCodeRule(2, {'[', 'B'}),
     EscapeCodeRule(3, {'[', 'C'}), EscapeCodeRule(4, {'[', 'D'}),
     EscapeCodeRule(5, {'[', 'E'}), EscapeCodeRule(6, {'[', 'F'}),
     EscapeCodeRule(7, {'[', 'G'}), EscapeCodeRule(8, {'[', 'H'}),
     // Mode setting
     EscapeCodeRule(9, {'[', '?', NumList, 'h'}),
     EscapeCodeRule(9, {'[', '?', Num, 'h'}),
     EscapeCodeRule(10, {'[', '?', NumList, 'l'}),
     EscapeCodeRule(10, {'[', '?', Num, 'l'}),
     // RGB and indexed colors (colon-separated)
     // Character sets
     EscapeCodeRule(11, {'(', 'A'}), EscapeCodeRule(11, {'(', 'B'}),
     EscapeCodeRule(11, {'(', 'C'}), EscapeCodeRule(11, {'(', '5'}),
     EscapeCodeRule(11, {'(', 'H'}), EscapeCodeRule(11, {'(', '7'}),
     EscapeCodeRule(11, {'(', 'K'}), EscapeCodeRule(11, {'(', 'Q'}),
     EscapeCodeRule(11, {'(', '9'}), EscapeCodeRule(11, {'(', 'R'}),
     EscapeCodeRule(11, {'(', 'f'}), EscapeCodeRule(11, {'(', 'Y'}),
     EscapeCodeRule(11, {'(', 'Z'}), EscapeCodeRule(11, {'(', '4'}),
     EscapeCodeRule(11, {'(', '='}), EscapeCodeRule(11, {'(', '`'}),
     EscapeCodeRule(11, {'(', 'E'}), EscapeCodeRule(11, {'(', '0'}),
     EscapeCodeRule(11, {'(', '<'}), EscapeCodeRule(11, {'(', '>'}),
     EscapeCodeRule(11, {'(', 'I'}), EscapeCodeRule(11, {'(', 'J'}),
     EscapeCodeRule(11, {'(', '"', '>'}), EscapeCodeRule(11, {'(', '"', '4'}),
     EscapeCodeRule(11, {'(', '"', '?'}), EscapeCodeRule(11, {'(', '%', '0'}),
     EscapeCodeRule(11, {'(', '%', '5'}), EscapeCodeRule(11, {'(', '%', '3'}),
     EscapeCodeRule(11, {'(', '%', '2'}), EscapeCodeRule(11, {'(', '%', '6'}),
     EscapeCodeRule(11, {'(', '%', '='}), EscapeCodeRule(11, {'(', '&', '4'}),
     EscapeCodeRule(11, {'(', '&', '5'}),
     EscapeCodeRule(12, {'[', NumList, 'H'}),
     EscapeCodeRule(12, {'[', NumList, 'f'}),
     EscapeCodeRule(13, {'[', Num, 'J'}), EscapeCodeRule(13, {'[', 'J'}),
     EscapeCodeRule(14, {'[', Num, 'K'}), EscapeCodeRule(14, {'[', 'K'}),
     EscapeCodeRule(15, {Num}), EscapeCodeRule(16, {'M'}),
     EscapeCodeRule(17, {'[', NumList, 'r'}),
     EscapeCodeRule(18, {'[', Num, 'L'}), EscapeCodeRule(18, {'[', 'L'}),
     EscapeCodeRule(19, {'[', Num, 'M'}), EscapeCodeRule(19, {'[', 'M'}),
     EscapeCodeRule(20, {'[', Num, '@'}), EscapeCodeRule(20, {'[', '@'}),
     EscapeCodeRule(21, {'[', Num, 'P'}), EscapeCodeRule(21, {'[', 'P'}),
     EscapeCodeRule(22, {'[', Num, 'X'}), EscapeCodeRule(22, {'[', 'X'}),
     EscapeCodeRule(23, {'[', 'S'}), EscapeCodeRule(23, {'[', Num, 'S'}),
     EscapeCodeRule(24, {'[', 'T'}), EscapeCodeRule(24, {'[', Num, 'T'}),
     EscapeCodeRule(25, {'[', 'd'}), EscapeCodeRule(25, {'[', Num, 'd'}),
     EscapeCodeRule(26, {'[', 'e'}), EscapeCodeRule(26, {'[', Num, 'e'}),
     EscapeCodeRule(27, {'[', Num, 'h'}), EscapeCodeRule(28, {'[', Num, 'l'}),
     EscapeCodeRule(29, {']', Num, ';', Str}),
     EscapeCodeRule(30, {'P', '+', 'Q', Str}),
     EscapeCodeRule(31, {'P', '+', 'q', Str}),
     EscapeCodeRule(32, {'[', Num, ' ', 'q'})}};

void Dfa::AddRule(const EscapeCodeRule &rule) {
  DfaState prev_state = 0;

  for (size_t i = 0; i < rule.tokens.size(); i++) {
    DfaToken tok = rule.tokens[i];

    DfaState num;

    if (Transition transition = states_.at(prev_state).NextByDfaToken(tok);
        transition.exists) {
      if (transition.accept) throw std::runtime_error("Unsupported grammar");

      num = transition.number;
    } else {
      num = AddState(State());

      State &prev_state_obj = states_.at(prev_state);

      if (i == rule.tokens.size() - 1)
        prev_state_obj.AddTransition(
            tok, Transition{.number = rule.rule_num, .exists = 1, .accept = 1});
      else
        prev_state_obj.AddTransition(
            tok, Transition{.number = num, .exists = 1, .accept = 0});
    }

    prev_state = num;
  }
}

Dfa::Dfa(const std::vector<EscapeCodeRule> &rules) {
  AddState(State());

  for (const auto &rule : rules) AddRule(rule);
}

Transition Dfa::Eat(DfaState curr_state, Token token) const {
  const State &curr = states_.at(curr_state);
  return curr.Next(token);
}

DfaState Dfa::AddState(State state) {
  states_.push_back(state);
  return states_.size() - 1;
}

EatResult EscapeParser::PushToken(Token &&tok) {
  result_.tokens.push_back(tok);

  const auto &tok_copy = result_.tokens.back();

  current_token_type_ = kNone;

  auto transition = dfa.Eat(dfa_state_, tok_copy);

  if (transition.accept) {
    result_.rule_num = transition.number;
    dfa_state_ = 0;
    result_ready_ = true;
    return EatResult::kAccept;
  } else if (!transition.exists) {
    dfa_state_ = 0;
    result_ = {};
    return EatResult::kError;
  } else
    dfa_state_ = transition.number;

  return EatResult::kNone;
}

EatResult EscapeParser::EatByte(char byte) {
  switch (current_token_type_) {
    case kNone:
      if ('0' <= byte && byte <= '9') {
        current_token_ = NumParseState{uint32_t(byte - '0'), {}};
        current_token_type_ = kNumber;
      } else if (dfa.HasStringTransition(dfa_state_)) {
        current_token_ = StrParseState{"", false};
        current_token_type_ = kString;
        return EatByte(byte);
      } else
        return PushToken((char)byte);

      break;

    case kString: {
      auto &state = std::get<StrParseState>(current_token_);

      if (byte != '\x07' &&  // BEL is a terminator
          (!state.prev_was_escape || byte != '\\')) { // so is ESC '\'
        state.str += byte;
        state.prev_was_escape = byte == '\e';
      } else {
        if (byte == '\\' && state.prev_was_escape) state.str.pop_back();

        switch (auto res = PushToken(state.str)) {
          case EatResult::kError:
          case EatResult::kAccept:
            return res;
          default:
            return EatByte(byte);
        }
      }

      break;
    }

    case kNumber:
    case kListOfNums: {
      auto &state = std::get<NumParseState>(current_token_);

      if ('0' <= byte && byte <= '9') {
        state.num *= 10;
        state.num += byte - '0';
      } else if (dfa.HasNumListTransition(dfa_state_) &&
                 (byte == ';' || byte == ':')) {
        if (current_token_type_ != kListOfNums) {
          state.num_list = {state.num};
          current_token_type_ = kListOfNums;
        } else
          state.num_list.push_back(state.num);
        state.num = 0;
      } else {
        if (current_token_type_ == kListOfNums)
          state.num_list.push_back(state.num);

        auto res = current_token_type_ == kListOfNums
                       ? PushToken(state.num_list)
                       : PushToken(state.num);
        switch (res) {
          case EatResult::kError:
            return res;
          case EatResult::kAccept:
            return EatResult::kAcceptButLastByteIsExtra;
          default:
            return EatByte(byte);
        }
      }

      break;
    }
  }

  return EatResult::kNone;
}

EscapeParseResult EscapeParser::Result() {
  if (result_ready_) {
    EscapeParseResult result;
    std::swap(result, result_);
    result_ready_ = false;
    return result;
  }

  throw std::runtime_error("Parser result was not ready");
}
}  // namespace bitty
