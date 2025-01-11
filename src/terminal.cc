#include "terminal.hh"

#include <GLFW/glfw3.h>

#include <fstream>
#include <memory>
#include <unordered_map>

#include "cell_buffer.hh"
#include "escape_parser.hh"
#include "font_renderer.hh"
#include "util.hh"

namespace bitty {
std::unordered_map<int, std::shared_ptr<Terminal>> Terminal::terminals_;

void Terminal::MakeBuffer() {
  auto [w, h] = std::tuple{140u, 35u};
  default_fg_ = Color(255, 255, 255, 255);
  default_bg_ = Color(255, 0, 0, 0);
  current_fg_ = default_fg_;
  current_bg_ = default_bg_;
  scroll_area_.left = 0;
  scroll_area_.top = 0;
  scroll_area_.right = w;
  scroll_area_.bottom = h;
  normal_buf_ = std::make_shared<CellBuffer>(w, h, h);
  alternate_buf_ = std::make_shared<CellBuffer>(w, h, h);
  buf_ = normal_buf_;
  SetWindowSize(w, h);
}

void Terminal::SwitchToAlternateBuffer() {
  if (buf_ != alternate_buf_) {
    buf_ = alternate_buf_;
    normal_cursor_x_ = cursor_x_;
    normal_cursor_y_ = cursor_y_;
    cursor_x_ = 0;
    cursor_y_ = 0;

    buf_->FillArea(GetDefaultScrollArea(), GetDefaultEmptyCell());
  }
}

Rect<u32> Terminal::GetDefaultScrollArea() {
  return Rect<u32>{0, 0, buf_->Width(), buf_->VisibleHeight()};
}

bool Terminal::IsUsingNormalBuffer() { return buf_ == normal_buf_; }

void Terminal::SwitchToNormalBuffer() {
  if (buf_ != normal_buf_) {
    buf_ = normal_buf_;

    SetCursor(normal_cursor_x_, normal_cursor_y_);
    buf_->MarkAllAsDirty();
  }
}

bool Terminal::TryScrollBufferUp(u32 pixels) {
  if (buf_ != normal_buf_) return false;

  buf_->UserScrollByNPixels(-pixels);

  return true;
}

bool Terminal::TryScrollBufferDown(u32 pixels) {
  if (buf_ != normal_buf_) return false;

  buf_->UserScrollByNPixels(pixels);

  return true;
}

bool Terminal::TryResetUserScroll() {
  if (buf_ != normal_buf_) return false;

  buf_->ResetUserScroll();

  return true;
}

bool Terminal::IsUserScrolledUp() {
  if (buf_ != normal_buf_) return false;

  return buf_->UserScrolledUp();
}

void Terminal::ReportMouseEvent(u32 btn, bool is_down, bool is_motion, u32 mods,
                                u32 x, u32 y) {
  (void)x;
  (void)y;

  if (mouse_tracking_format_ == MouseTrackingFormat::kX10Compat && !btn) return;

  char mouse_button_encoded;

  if ((mouse_tracking_format_ == MouseTrackingFormat::kNormal ||
       mouse_tracking_format_ == MouseTrackingFormat::kX10Compat) &&
      !is_down)
    mouse_button_encoded = 3;
  else {
    mouse_button_encoded = btn - 1;

    switch (btn) {
      case 4:
      case 5:
      case 6:
      case 7:
        mouse_button_encoded -= 4;
        mouse_button_encoded |= 1 << 6;
        break;
      case 8:
      case 9:
      case 10:
      case 11:
        mouse_button_encoded -= 8;
        mouse_button_encoded |= 1 << 7;
        break;
    }
  }

  if (mods & GLFW_MOD_SHIFT) mouse_button_encoded |= 1 << 2;
  if (mods & GLFW_MOD_SUPER) mouse_button_encoded |= 1 << 3;
  if (mods & GLFW_MOD_CONTROL) mouse_button_encoded |= 1 << 4;

  u32 mouse_x = mouse_pos_x_;
  u32 mouse_y = mouse_pos_y_;

  switch (mouse_tracking_format_) {
    case MouseTrackingFormat::kNormal:
    case MouseTrackingFormat::kX10Compat: {
      mouse_x /= GlobalCellWidthPx();
      mouse_y /= GlobalCellHeightPx();

      bool add_bit_5 =
          mouse_tracking_format_ == MouseTrackingFormat::kX10Compat ||
          is_motion || mouse_mode_ <= MouseTrackingMode::kOnlyButtonEvents;

      WriteToPty({'\e', '[', 'M', char(mouse_button_encoded + 32 * add_bit_5),
                  (char)std::min(255u, u32(32 + mouse_x + 1)),
                  (char)std::min(255u, u32(32 + mouse_y + 1))});
      break;
    }
    case MouseTrackingFormat::kSGRPixels:
    case MouseTrackingFormat::kSGR: {
      if (mouse_tracking_format_ == MouseTrackingFormat::kSGR) {
        mouse_x /= GlobalCellWidthPx();
        mouse_y /= GlobalCellHeightPx();
      }

      if (is_motion) mouse_button_encoded += 32;

      auto str = std::format("\e[<{};{};{};{}", (u32)mouse_button_encoded,
                             mouse_x + 1, mouse_y + 1, is_down ? 'M' : 'm');
      std::vector<char> bytes(str.begin(), str.end());
      WriteToPty(std::move(bytes));
      break;
    }
    default:
      break;
  }
}

void Terminal::HandleMouseScroll(const EventMouseScroll& event) {
  int scroll_unit = GlobalCellHeightPx() * 2;

  if (int scroll_px = std::round(event.offset_y); scroll_px < 0)
    TryScrollBufferDown(-scroll_px * scroll_unit);
  else
    TryScrollBufferUp(scroll_px * scroll_unit);

  if (mouse_mode_ >= MouseTrackingMode::kOnlyButtonEvents) {
    if (int oy = (int)event.offset_y; oy != 0)
      ReportMouseEvent(oy > 0 ? 5 : 6, true, false, mouse_mods_, mouse_pos_x_,
                       mouse_pos_y_);

    if (int ox = (int)event.offset_x; ox != 0) {
      LogInfo() << ox << '\n';
      ReportMouseEvent(ox > 0 ? 7 : 8, true, false, mouse_mods_, mouse_pos_x_,
                       mouse_pos_y_);
    }
  }
}

void Terminal::HandleMousePos(const EventMousePos& event) {
  if (event.new_pos_x >= 0 && event.new_pos_y >= 0) {
    u32 x = event.new_pos_x;
    u32 y = event.new_pos_y;

    mouse_pos_x_ = x;
    mouse_pos_y_ = y;

    if ((mouse_mode_ == MouseTrackingMode::kMotionEventsIfMouseDown &&
         mouse_down_) ||
        mouse_mode_ == MouseTrackingMode::kAllEvents) {
      ReportMouseEvent(mouse_btn_, mouse_down_, true, mouse_mods_, x, y);
    }
  }
}

void Terminal::HandleMouseButton(const EventMouseButton& event) {
  if (mouse_mode_ >= MouseTrackingMode::kOnlyButtonEvents) {
    bool pressed = event.action == GLFW_PRESS;

    uint32_t btn = 0;

    switch (event.button) {
      case GLFW_MOUSE_BUTTON_1:
        btn = 1;
        break;
      case GLFW_MOUSE_BUTTON_2:
        btn = 2;
        break;
      case GLFW_MOUSE_BUTTON_3:
        btn = 3;
        break;
      default:
        LogError() << "Unhandled mouse event: pressed = " << pressed
                   << ", button = " << event.button << '\n';
        return;
    }

    mouse_mods_ = event.mods;
    mouse_btn_ = btn;
    mouse_down_ = pressed;

    ReportMouseEvent(btn, mouse_down_, false, mouse_mods_, mouse_pos_x_,
                     mouse_pos_y_);
  }
}

int Terminal::Create(const std::string& shell_path) {
  try {
    auto term = std::shared_ptr<Terminal>(new Terminal(shell_path));

    int id = term->pt_master_no_;

    term->AssignId(id);

    terminals_[id] = std::move(term);

    return id;
  } catch (...) {
    return -1;
  }
}

void Terminal::ReportUnhandledSequence() {
#ifdef TERM_DEBUG
  LogError() << "Unhandled ANSI escape sequence #" << esc_seq_error_counter_++
             << ": "
             << "\\e" << last_escape_seq_ << '\n';
#endif
}

void Terminal::ReportUnparsedSequence() {
#ifdef TERM_DEBUG
  LogError() << "Unparsed ANSI escape sequence #" << esc_seq_error_counter_++
             << ": "
             << "\\e" << last_escape_seq_ << "...\n";
#endif
}

void Terminal::HandleIndividualModifierForMSequence(u32 mod) {
  switch (mod) {
    case 0:
      ResetFgColor();
      ResetBgColor();
      ResetCellFlags(CellFlags::kAll);
      break;
    case 1:
      SetCellFlags(CellFlags::kBold);
      break;
    case 3:
      SetCellFlags(CellFlags::kItalic);
      break;
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
      SetFgColor(color_table_256_.colors[mod - 30]);
      break;
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
      SetFgColor(color_table_256_.colors[mod - 90 + 8]);
      break;
    case 39:
      ResetFgColor();
      break;
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      SetBgColor(color_table_256_.colors[mod - 40]);
      break;
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
      SetBgColor(color_table_256_.colors[mod - 100 + 8]);
      break;
    case 49:
      ResetBgColor();
      break;
    default:
      // ReportUnhandledSequence();
      break;
  }
}

// clang-format off
DEF_ESC_HANDLER(ChangeFormatting) {
  if (tokens.size() == 2)
    HandleIndividualModifierForMSequence(0);
  else
    std::visit(
      Overloaded{
        [&](const std::vector<u32>& prms) mutable {
          if (prms.size() >= 1 && (prms[0] == 38 || prms[0] == 48)) {
            if (prms.size() == 3)
              SetIndexedColor(prms[0], prms[2]);
            else if (prms.size() == 5)
              SetRgbColor(prms[0], prms[2], prms[3], prms[4]);
            else if (prms.size() == 6)
              SetRgbColor(prms[0], prms[3], prms[4], prms[5]);
          }
          else for (u32 mod : prms)
            HandleIndividualModifierForMSequence(mod);
         return;
        },
        [&](u32 mod) {
          HandleIndividualModifierForMSequence(mod);
        },
        [&](auto) {
          ReportUnhandledSequence();
        }},
      tokens.at(1));
}
// clang-format on

#define CURSOR_OFFSET \
  std::clamp((tokens.size() == 3 ? std::get<u32>(tokens.at(1)) : 1), 1u, 9999u)

DEF_ESC_HANDLER(MoveCursorUp) {
  u32 off = CURSOR_OFFSET;

  u32 limit = CursorY() < scroll_area_.top ? 0 : scroll_area_.top;

  SetCursorY(std::max(limit, std::sub_sat(CursorY(), off)));
}

DEF_ESC_HANDLER(MoveCursorDown) {
  u32 off = CURSOR_OFFSET;

  u32 limit = CursorY() >= scroll_area_.bottom ? buf_->VisibleHeight() - 1
                                               : scroll_area_.bottom - 1;

  SetCursorY(std::min(limit, std::add_sat(CursorY(), off)));
}

DEF_ESC_HANDLER(MoveCursorRight) {
  u32 off = CURSOR_OFFSET;

  SetCursorX(std::min(buf_->Width() - 1, std::add_sat(CursorX(), off)));
}

DEF_ESC_HANDLER(MoveCursorLeft) {
  u32 off = CURSOR_OFFSET;

  SetCursorX(std::sub_sat(CursorX(), off));
}

DEF_ESC_HANDLER(MoveCursorToX0NLinesDown) {
  SetCursorX(0);
  MoveCursorDown(std::move(tokens));
}

DEF_ESC_HANDLER(MoveCursorToX0NLinesUp) {
  SetCursorX(0);
  MoveCursorUp(std::move(tokens));
}

DEF_ESC_HANDLER(MoveCursorToColumn) {
  u32 offset = CURSOR_OFFSET;
  if (offset > 0) SetCursorX(offset - 1);
}

DEF_ESC_HANDLER(MoveCursorTo00) {
  (void)tokens;
  SetCursor(0, 0);
}

void Terminal::SetPrivateMode(u32 mode, bool flag) {
  switch (mode) {
    case 7:
      SetAutowrap(flag);
      break;  // Autowrap
    case 9:
      if (flag) {
        mouse_mode_ = MouseTrackingMode::kOnlyButtonEvents;
        mouse_tracking_format_ = MouseTrackingFormat::kX10Compat;
      } else
        mouse_mode_ = MouseTrackingMode::kNoTracking;
      break;
    case 25:
      SetCursorVisibility(flag);
      break;
    case 45:
      SetReverseWraparound(flag);
      break;
    case 1000:
    case 1002:
    case 1003:
      if (flag) {
        mouse_mode_ = std::max(
            mouse_mode_, mode == 1000 ? MouseTrackingMode::kOnlyButtonEvents
                         : mode == 1002
                             ? MouseTrackingMode::kMotionEventsIfMouseDown
                         : mode == 1003 ? MouseTrackingMode::kAllEvents
                                        : MouseTrackingMode::kNoTracking);
      } else
        mouse_mode_ = MouseTrackingMode::kNoTracking;
      break;
    case 1005:
      mouse_tracking_format_ =
          flag ? MouseTrackingFormat::kUTF8 : MouseTrackingFormat::kNormal;
      break;
    case 1006:
      mouse_tracking_format_ =
          flag ? MouseTrackingFormat::kSGR : MouseTrackingFormat::kNormal;
      break;
    case 1015:
      mouse_tracking_format_ =
          flag ? MouseTrackingFormat::kURXVT : MouseTrackingFormat::kNormal;
      break;
    case 1016:
      mouse_tracking_format_ =
          flag ? MouseTrackingFormat::kSGRPixels : MouseTrackingFormat::kNormal;
      break;
      // Alternate screen buffer
    case 1049:
      if (flag)
        SwitchToAlternateBuffer();
      else
        SwitchToNormalBuffer();
      break;
    default:
      ReportUnhandledSequence();
      break;
  }
}

void Terminal::SetMode(u32 mode, bool flag) {
  switch (mode) {
    case 20:
      SetLNM(flag);
      break;
    default:
      ReportUnhandledSequence();
      break;
  }
}

void Terminal::ChangeModeSettings(const std::vector<Token>& tokens, bool flag) {
  std::visit(Overloaded{[&](const std::vector<u32>& modes) mutable {
                          for (uint32_t mode : modes)
                            SetPrivateMode(mode, flag);
                        },
                        [&](u32 mode) mutable { SetPrivateMode(mode, flag); },
                        [](auto) mutable {}},
             tokens.at(2));
}

DEF_ESC_HANDLER(DecPrivateModeSet) { ChangeModeSettings(tokens, true); }

DEF_ESC_HANDLER(DecPrivateModeReset) { ChangeModeSettings(tokens, false); }

DEF_ESC_HANDLER(DecModeSet) {
  u32 mode = std::get<u32>(tokens.at(1));

  SetMode(mode, true);
}

DEF_ESC_HANDLER(DecModeReset) {
  u32 mode = std::get<u32>(tokens.at(1));

  SetMode(mode, false);
}

DEF_ESC_HANDLER(SetCursorPosition) {
  const auto& pos = std::get<std::vector<u32>>(tokens.at(1));

  if (pos.size() >= 2 && pos[1] > 0 && pos[0] > 0)
    SetCursor(pos[1] - 1, pos[0] - 1);
}

void Terminal::SetIndexedColor(u32 fg_or_bg, u32 color) {
  bool bg = fg_or_bg == 48;

  Color col = color_table_256_.colors[color % 256];

  if (bg)
    SetBgColor(col);
  else
    SetFgColor(col);
}

void Terminal::SetCellFlags(CellFlags flags) {
  current_cell_flags_ = (CellFlags)(current_cell_flags_ | flags);
}

void Terminal::ResetCellFlags(CellFlags flags) {
  current_cell_flags_ = (CellFlags)(current_cell_flags_ & ~flags);
}

void Terminal::ToggleCellFlags(CellFlags flags) {
  current_cell_flags_ = (CellFlags)(current_cell_flags_ ^ flags);
}

void Terminal::SetRgbColor(u32 fg_or_bg, u32 r, u32 g, u32 b) {
  bool bg = fg_or_bg == 48;

  Color col(255, r, g, b);
  if (bg)
    SetBgColor(col);
  else
    SetFgColor(col);
}

DEF_ESC_HANDLER(SetCharacterSet) { (void)tokens; }

DEF_ESC_HANDLER(SetCursorStyleHandler) {
  (void)tokens;
  ReportUnhandledSequence();
}

DEF_ESC_HANDLER(ClearScreen) {
  auto space = GetDefaultEmptyCell();

  auto clear_from_start_of_screen = [this, space]() {
    buf_->FillArea(Rect<u32>{0, 0, buf_->Width(), CursorY() - 1}, space);

    buf_->FillLine(0, CursorX() + 1, CursorY(), space);
  };

  auto clear_to_end_of_screen = [this, space]() {
    buf_->FillArea(
        Rect<u32>{0, CursorY() + 1, buf_->Width(), buf_->VisibleHeight()},
        space);

    buf_->FillLine(CursorX(), buf_->Width(), CursorY(), space);
  };

  if (tokens.size() == 2)
    clear_to_end_of_screen();
  else
    switch (std::get<u32>(tokens.at(1))) {
      case 0:
        clear_to_end_of_screen();
        break;
      case 1:
        clear_from_start_of_screen();
        break;
      case 2:
        clear_to_end_of_screen();
        clear_from_start_of_screen();
        break;
      default:
        ReportUnhandledSequence();
        break;
    }
}

DEF_ESC_HANDLER(ClearLine) {
  auto space = GetDefaultEmptyCell();

  auto clear_from_start_of_line = [this, space]() {
    buf_->FillLine(0, CursorX(), CursorY(), space);
  };

  auto clear_to_end_of_line = [this, space]() {
    buf_->FillLine(CursorX(), buf_->Width(), CursorY(), space);
  };

  if (tokens.size() == 2)
    clear_to_end_of_line();
  else
    switch (std::get<u32>(tokens.at(1))) {
      case 0:
        clear_to_end_of_line();
        break;
      case 1:
        clear_from_start_of_line();
        break;
      case 2:
        clear_to_end_of_line();
        clear_from_start_of_line();
        break;
      default:
        ReportUnhandledSequence();
        break;
    }
}

DEF_ESC_HANDLER(ReverseIndexHandler) {
  (void)tokens;
  ReverseIndex();
}

DEF_ESC_HANDLER(SetVerticalScrollingHandler) {
  const auto& vec = std::get<std::vector<u32>>(tokens.at(1));

  if (vec.size() != 2) return;

  u32 top = vec[0];
  u32 bottom = vec[1];

  LogInfo() << "Set scrolling margins: " << top << ";" << bottom << '\n';

  SetCursor(0, 0);

  if (top > 0 && bottom > 0 && bottom > top &&
      bottom <= buf_->VisibleHeight()) {
    scroll_area_.top = top - 1;
    scroll_area_.bottom = bottom;
  }
}

void Terminal::SaveCursorPosition() {
  saved_cursor_x_ = CursorX();
  saved_cursor_y_ = CursorY();
}

void Terminal::RestoreCursorPosition() {
  SetCursor(saved_cursor_x_, saved_cursor_y_);
}

DEF_ESC_HANDLER(EscThenNumberHandler) {
  switch (std::get<u32>(tokens.at(0))) {
    case 7:
      SaveCursorPosition();
      break;
    case 8:
      RestoreCursorPosition();
      break;
    default:
      ReportUnhandledSequence();
      break;
  }
}

void Terminal::InsertNLinesAt(u32 y, u32 n) {
  buf_->CopyArea(Rect{scroll_area_.left, y, scroll_area_.right,
                      std::sub_sat(scroll_area_.bottom, n)},
                 Rect{
                     scroll_area_.left,
                     std::add_sat(y, n),
                     scroll_area_.right,
                     scroll_area_.bottom,
                 });

  buf_->FillArea(
      Rect{scroll_area_.left, y, scroll_area_.right, std::add_sat(y, n)},
      GetDefaultEmptyCell());
}

void Terminal::DeleteNLinesAt(u32 y, u32 n) {
  buf_->CopyArea(Rect{scroll_area_.left, std::add_sat(y, n), scroll_area_.right,
                      scroll_area_.bottom},
                 Rect{
                     scroll_area_.left,
                     y,
                     scroll_area_.right,
                     std::sub_sat(scroll_area_.bottom, n),
                 });

  buf_->FillArea(Rect{scroll_area_.left, std::sub_sat(scroll_area_.bottom, n),
                      scroll_area_.right, scroll_area_.bottom},
                 GetDefaultEmptyCell());
}

DEF_ESC_HANDLER(InsertNLines) {
  u32 n = CURSOR_OFFSET;

  InsertNLinesAt(CursorY(), n);
}

DEF_ESC_HANDLER(DeleteNLines) {
  u32 n = CURSOR_OFFSET;

  DeleteNLinesAt(CursorY(), n);
}

DEF_ESC_HANDLER(PanDown) {
  u32 n = CURSOR_OFFSET;

  if (IsUsingNormalBuffer() && scroll_area_ == GetDefaultScrollArea()) {
    buf_->ScrollByNCells(n, true);
  } else {
    if (n >= scroll_area_.Height()) {
      buf_->FillArea(scroll_area_, GetDefaultEmptyCell());
    } else {
      buf_->CopyArea(
          Rect{scroll_area_.left, std::add_sat(scroll_area_.top, n),
               scroll_area_.right, scroll_area_.bottom},
          Rect{scroll_area_.left, scroll_area_.top, scroll_area_.right,
               std::sub_sat(scroll_area_.bottom, n)});
      buf_->FillArea(
          Rect{scroll_area_.left, std::sub_sat(scroll_area_.bottom, n),
               scroll_area_.right, scroll_area_.bottom},
          GetDefaultEmptyCell());
    }
  }
}

DEF_ESC_HANDLER(PanUp) {
  u32 n = CURSOR_OFFSET;

  if (IsUsingNormalBuffer() && scroll_area_ == GetDefaultScrollArea())
    buf_->ScrollByNCells(-n, false);
  else
    InsertNLinesAt(scroll_area_.top, n);
}

DEF_ESC_HANDLER(VerticalLinePositionAbsolute) {
  u32 n = CURSOR_OFFSET;
  SetCursorY(std::min(buf_->VisibleHeight(), n - 1));
}

DEF_ESC_HANDLER(VerticalLinePositionRelative) {
  u32 n = CURSOR_OFFSET;
  SetCursorY(std::min(buf_->VisibleHeight(), std::add_sat(CursorY(), n)));
}

DEF_ESC_HANDLER(EraseNCharacters) {
  u32 n = CURSOR_OFFSET;

  if (n == 0) n = 1;

  buf_->FillLine(CursorX(),
                 std::min(scroll_area_.right, std::add_sat(CursorX(), n)),
                 CursorY(), GetDefaultEmptyCell());
}

DEF_ESC_HANDLER(InsertNCharacters) {
  u32 n = CURSOR_OFFSET;

  (void)n;

  ReportUnhandledSequence();
}

DEF_ESC_HANDLER(DeleteNCharacters) {
  u32 n = CURSOR_OFFSET;

  u32 left = std::add_sat(CursorX(), n);
  u32 right = scroll_area_.right;

  if (right > left) {
    u32 middle = std::sub_sat(right, n);

    buf_->CopyArea(Rect{left, CursorY(), right, CursorY() + 1},
                   Rect{CursorX(), CursorY(), middle, CursorY() + 1});

    buf_->FillLine(middle, right, CursorY(), GetDefaultEmptyCell());
  } else
    buf_->FillLine(CursorX(), right, CursorY(), GetDefaultEmptyCell());
}

#undef CURSOR_OFFSET

DEF_ESC_HANDLER(GeneralOscHandler) {
  (void)tokens;
  ReportUnhandledSequence();
}

DEF_ESC_HANDLER(RequestResValuesHandler) {
  (void)tokens;
  ReportUnhandledSequence();
}

DEF_ESC_HANDLER(RequestTerminfoHandler) {
  (void)tokens;
  ReportUnhandledSequence();
}

void Terminal::SetCursorVisibility(bool flag) { is_cursor_visible_ = flag; }

bool Terminal::IsCursorVisible() { return is_cursor_visible_; }

void Terminal::ResetFgColor() { current_fg_ = default_fg_; }

void Terminal::ResetBgColor() { current_bg_ = default_bg_; }

void Terminal::SetReverseWraparound(bool flag) { reverse_wraparound_ = flag; }

void Terminal::SetAutowrap(bool flag) { forward_wraparound_ = flag; }

void Terminal::SetLNM(bool flag) { lnm_flag_ = flag; }

bool Terminal::IsLNMSet() { return lnm_flag_; }

bool Terminal::IsReverseWraparoundEnabled() { return reverse_wraparound_; }

bool Terminal::IsAutowrapEnabled() { return forward_wraparound_; }

void Terminal::SetFgColor(Color col) { current_fg_ = col; }

void Terminal::SetBgColor(Color col) { current_bg_ = col; }

u32 Terminal::CursorX() const { return cursor_x_; }
u32 Terminal::CursorY() const { return cursor_y_; }

void Terminal::SetCursor(u32 x, u32 y) {
  cursor_x_ = x;
  cursor_y_ = y;
}

void Terminal::SetCursorX(u32 x) { SetCursor(x, cursor_y_); }

void Terminal::SetCursorY(u32 y) { SetCursor(cursor_x_, y); }

void Terminal::GoForwardX() { SetCursorX(CursorX() + 1); }

void Terminal::GoBackX() {
  if (CursorX() == scroll_area_.left) {
    if (IsReverseWraparoundEnabled()) {
      SetCursorX(scroll_area_.right - 1);
      SetCursorY(CursorY() - 1);
    }
  } else
    SetCursorX(CursorX() - 1);
}

void Terminal::CarriageReturn() { SetCursorX(0); }

void Terminal::ReverseIndex() {
  if (CursorY() == scroll_area_.top) {
    if (IsUsingNormalBuffer()) {
      buf_->ScrollByNCells(-1, false);
    } else {
      buf_->CopyArea(Rect{scroll_area_.left, scroll_area_.top,
                          scroll_area_.right, scroll_area_.bottom - 1},
                     Rect{scroll_area_.left, scroll_area_.top + 1,
                          scroll_area_.right, scroll_area_.bottom});
      buf_->FillLine(scroll_area_.left, scroll_area_.right, scroll_area_.top,
                     GetDefaultEmptyCell());
    }
  } else
    SetCursorY(CursorY() - 1);
}

void Terminal::LineFeed() {
  if (IsLNMSet()) SetCursorX(scroll_area_.left);

  if (CursorY() == scroll_area_.bottom - 1) {
    if (IsUsingNormalBuffer()) {
      buf_->ScrollByNCells(1, true);
    } else {
      buf_->CopyArea(Rect{scroll_area_.left, scroll_area_.top + 1,
                          scroll_area_.right, scroll_area_.bottom},
                     Rect{scroll_area_.left, scroll_area_.top,
                          scroll_area_.right, scroll_area_.bottom - 1});
      buf_->FillLine(scroll_area_.left, scroll_area_.right,
                     scroll_area_.bottom - 1, GetDefaultEmptyCell());
    }
  } else
    SetCursorY(CursorY() + 1);
}

bool Terminal::Set(ColoredCell chr) {
  return buf_->Set(cursor_x_, cursor_y_, chr);
}

bool Terminal::Set(Cell chr) {
  chr.flags = current_cell_flags_;
  return Set(ColoredCell(chr, current_fg_, current_bg_));
}

bool Terminal::Set(u32 x, u32 y, Cell chr) {
  chr.flags = current_cell_flags_;
  return buf_->Set(x, y, ColoredCell(chr, current_fg_, current_bg_));
}

void Terminal::InterpretPtyInput(char byte) {
#ifdef TERM_DEBUG
  static std::ofstream log("pty.log", std::ios_base::binary);
  log << byte;
  log.flush();
#endif

  if (parsing_escape_code_) {
    auto res = escape_parser_.EatByte(byte);

    if (byte != 0) last_escape_seq_ += byte;

    switch (res) {
      case EatResult::kAcceptButLastByteIsExtra:
      case EatResult::kAccept: {
        auto result = escape_parser_.Result();

        auto handler = escape_handlers.at(result.rule_num);
        (this->*handler)(std::move(result.tokens));

        parsing_escape_code_ = false;

        if (res == EatResult::kAcceptButLastByteIsExtra) {
          InterpretPtyInput(byte);
          return;
        }

        break;
      }
      case EatResult::kError:
        ReportUnparsedSequence();
        parsing_escape_code_ = false;
        break;
      default:
        break;
    }
  } else if (byte == '\e') {
    parsing_escape_code_ = true;
    last_escape_seq_ = "";
  } else if (byte == '\n' || byte == '\f' || byte == '\v')
    LineFeed();
  else if (byte == '\r')
    CarriageReturn();
  else if (byte == '\t')
    for (size_t i = 0; i < 4; i++) GoForwardX();
  else if (byte == '\b')
    GoBackX();
  else if (byte == '\x07')
    LogError() << "\\x07 wasn't handled\n";
  else if (byte == '\x0f')
    ;
  /*else if (byte == '\x9d' || byte == '\x9b' || byte == '\x84' ||
           byte == '\x85' || byte == '\x88' || byte == '\x8d' ||
           byte == '\x8e' || byte == '\x8f') {
    parsing_escape_code_ = true;
    last_escape_seq_ = "";

    // clang-format off
    switch (byte) {
      case '\x9d': escape_parser_.EatByte(']'); break;
      case '\x9b': escape_parser_.EatByte('['); break;
      case '\x84': escape_parser_.EatByte('D'); break;
      case '\x85': escape_parser_.EatByte('E'); break;
      case '\x88': escape_parser_.EatByte('H'); break;
      case '\x8d': escape_parser_.EatByte('M'); break;
      case '\x8e': escape_parser_.EatByte('N'); break;
      case '\x8f': escape_parser_.EatByte('O'); break;
      default: break;
    }
    // clang-format on

  }**/
  else if (u32 codepoint = utf8_parser_.Feed(byte); codepoint != -u32(1)) {
    bool dont_overwrite_with_space = false;

    if (codepoint == ' ') {
      if (auto maybe_cell = buf_->Get(CursorX() - 1, CursorY());
          maybe_cell.has_value()) {
        ColoredCell cell = maybe_cell.value();
        if (cell.segment_count > 1 &&
            cell.segment_index != cell.segment_count - 1)
          dont_overwrite_with_space = true;
      }
    }

    u32 segments =
        codepoint < 256
            ? 1
            : FontRenderer::Get().GetCodePointWidthInCells(codepoint);

    if (CursorX() >= scroll_area_.right) {
      if (IsAutowrapEnabled()) {
        CarriageReturn();
        LineFeed();
      } else
        SetCursorX(scroll_area_.right - 1);
    }

    if (!dont_overwrite_with_space)
      for (u32 i = 0; i < segments; i++)
        Set(CursorX() + i, CursorY(), Cell(codepoint, 0, i, segments));

    GoForwardX();
  }
}

std::optional<std::shared_ptr<Terminal>> Terminal::Get(int id) {
  if (auto found = terminals_.find(id); found != terminals_.end())
    return found->second;
  return std::nullopt;
}
}  // namespace bitty