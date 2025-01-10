#ifndef __BITTY_TERMINAL_HH__
#define __BITTY_TERMINAL_HH__

#include <boost/process.hpp>
#include <boost/process/v1/detail/child_decl.hpp>
#include <compare>

#include "cell.hh"
#include "escape_parser.hh"
#include "events.hh"
#include "utf8_parser.hh"
#include "util.hh"

namespace bitty {
class CellBuffer;

struct ColorTable256 {
  std::array<Color, 256> colors;
  constexpr static std::array<int, 6> coord_to_rgb_chan = {
      0, 95, 95 + 40, 95 + 80, 95 + 120, 95 + 160};

  consteval ColorTable256() {
    colors[0] = Color(255, 0x22, 0x22, 0x22);
    colors[8] = Color(255, 0x66, 0x66, 0x66);

    colors[0b0100] = Color(255, 0, 0x88, 0xCC);
    colors[0b1100] = Color(255, 0, 0xAA, 0xEE);

    for (unsigned i : {1, 2, 3, 5, 6, 7}) {
      colors[i + 0] = Color::Decode3BitColor(i & 0b111, 0xCC);
      colors[i + 8] = Color::Decode3BitColor(i & 0b111, 0xFF);
    }

    for (unsigned i = 0; i < 216; i++) {
      unsigned I = i;
      int b = coord_to_rgb_chan[I % 6];
      I /= 6;
      int g = coord_to_rgb_chan[I % 6];
      I /= 6;
      int r = coord_to_rgb_chan[I % 6];

      colors[i + 16] = Color(255, r, g, b);
    }

    float intensity = 0;

    for (unsigned i = 0; i < 24; i++, intensity += 255.f / 24)
      colors[i + 232] =
          Color(255, intensity + .5, intensity + .5, intensity + .5);
  }
};

#define DECL_ESC_HANDLER(name) void name(std::vector<Token> tokens);
#define DEF_ESC_HANDLER(name) void Terminal::name(std::vector<Token> tokens)
#define PTR_ESC_HANDLER &Terminal::

enum class CursorStyle { kBar, kLine };

enum MouseTrackingFormat {
  kNormal,
  kX10Compat,
  kUTF8,
  kSGR,
  kURXVT,
  kSGRPixels
};

enum class MouseTrackingMode {
  kNoTracking,
  kOnlyButtonEvents,
  kMotionEventsIfMouseDown,
  kAllEvents
};

inline std::strong_ordering operator<=>(MouseTrackingMode a,
                                        MouseTrackingMode b) {
  return (i32)a <=> (i32)b;
}

class Terminal {
  int pt_master_no_, event_fd_, id_{-1};
  std::thread thread_;
  std::shared_ptr<CellBuffer> buf_;
  std::shared_ptr<CellBuffer> normal_buf_, alternate_buf_;

  EscapeParser escape_parser_;
  Utf8Parser utf8_parser_;
  i32 saved_cursor_x_{0}, saved_cursor_y_{0};
  i32 normal_cursor_x_{0}, normal_cursor_y_{0};
  i32 esc_seq_error_counter_{0};
  Color current_fg_{0}, current_bg_{0}, default_fg_{0}, default_bg_{0};
  CellFlags current_cell_flags_{0};
  CursorStyle cursor_style_ = CursorStyle::kBar;
  bool is_cursor_visible_{true}, lnm_flag_{false};

  MouseTrackingFormat mouse_tracking_format_{MouseTrackingFormat::kNormal};
  MouseTrackingMode mouse_mode_{MouseTrackingMode::kNoTracking};
  bool mouse_down_ = false;
  u32 mouse_pos_x_ = 0, mouse_pos_y_ = 0;
  int mouse_mods_ = 0, mouse_btn_ = 0;

  Rect<u32> scroll_area_ = {};

  bool reverse_wraparound_{true}, forward_wraparound_{true};
  i32 cursor_x_{0}, cursor_y_{0};

  constexpr static ColorTable256 color_table_256_ = ColorTable256();
  static std::unordered_map<int, std::shared_ptr<Terminal>> terminals_;

  std::string last_escape_seq_ = "";

  bool parsing_escape_code_{false};

  constexpr static int kReadChunkSize = 16384;

  using EscapeCodeRuleHandler = void (Terminal::*)(std::vector<Token>);

  DECL_ESC_HANDLER(ChangeFormatting)
  DECL_ESC_HANDLER(MoveCursorUp)
  DECL_ESC_HANDLER(MoveCursorDown)
  DECL_ESC_HANDLER(MoveCursorRight)
  DECL_ESC_HANDLER(MoveCursorLeft)
  DECL_ESC_HANDLER(MoveCursorToX0NLinesDown)
  DECL_ESC_HANDLER(MoveCursorToX0NLinesUp)
  DECL_ESC_HANDLER(MoveCursorToColumn)
  DECL_ESC_HANDLER(MoveCursorTo00)
  DECL_ESC_HANDLER(DecPrivateModeSet)
  DECL_ESC_HANDLER(DecPrivateModeReset)
  DECL_ESC_HANDLER(SetCharacterSet)
  DECL_ESC_HANDLER(SetCursorPosition)
  DECL_ESC_HANDLER(ClearScreen)
  DECL_ESC_HANDLER(ClearLine)
  DECL_ESC_HANDLER(EscThenNumberHandler)
  DECL_ESC_HANDLER(ReverseIndexHandler)
  DECL_ESC_HANDLER(SetVerticalScrollingHandler)
  DECL_ESC_HANDLER(InsertNLines)
  DECL_ESC_HANDLER(DeleteNLines)
  DECL_ESC_HANDLER(InsertNCharacters)
  DECL_ESC_HANDLER(DeleteNCharacters)
  DECL_ESC_HANDLER(EraseNCharacters)
  DECL_ESC_HANDLER(PanDown)
  DECL_ESC_HANDLER(PanUp)
  DECL_ESC_HANDLER(VerticalLinePositionAbsolute)
  DECL_ESC_HANDLER(VerticalLinePositionRelative)
  DECL_ESC_HANDLER(DecModeSet)
  DECL_ESC_HANDLER(DecModeReset)
  DECL_ESC_HANDLER(GeneralOscHandler)
  DECL_ESC_HANDLER(RequestResValuesHandler)
  DECL_ESC_HANDLER(RequestTerminfoHandler)
  DECL_ESC_HANDLER(SetCursorStyleHandler)

  void SaveCursorPosition();
  void RestoreCursorPosition();
  void InsertNLinesAt(u32 y, u32 n);
  void DeleteNLinesAt(u32 y, u32 n);

  constexpr static std::array<EscapeCodeRuleHandler, kEscapeRuleCount>
      escape_handlers = {PTR_ESC_HANDLER ChangeFormatting,
                         PTR_ESC_HANDLER MoveCursorUp,
                         PTR_ESC_HANDLER MoveCursorDown,
                         PTR_ESC_HANDLER MoveCursorRight,
                         PTR_ESC_HANDLER MoveCursorLeft,
                         PTR_ESC_HANDLER MoveCursorToX0NLinesDown,
                         PTR_ESC_HANDLER MoveCursorToX0NLinesUp,
                         PTR_ESC_HANDLER MoveCursorToColumn,
                         PTR_ESC_HANDLER MoveCursorTo00,
                         PTR_ESC_HANDLER DecPrivateModeSet,
                         PTR_ESC_HANDLER DecPrivateModeReset,
                         PTR_ESC_HANDLER SetCharacterSet,
                         PTR_ESC_HANDLER SetCursorPosition,
                         PTR_ESC_HANDLER ClearScreen,
                         PTR_ESC_HANDLER ClearLine,
                         PTR_ESC_HANDLER EscThenNumberHandler,
                         PTR_ESC_HANDLER ReverseIndexHandler,
                         PTR_ESC_HANDLER SetVerticalScrollingHandler,
                         PTR_ESC_HANDLER InsertNLines,
                         PTR_ESC_HANDLER DeleteNLines,
                         PTR_ESC_HANDLER InsertNCharacters,
                         PTR_ESC_HANDLER DeleteNCharacters,
                         PTR_ESC_HANDLER EraseNCharacters,
                         PTR_ESC_HANDLER PanDown,
                         PTR_ESC_HANDLER PanUp,
                         PTR_ESC_HANDLER VerticalLinePositionAbsolute,
                         PTR_ESC_HANDLER VerticalLinePositionRelative,
                         PTR_ESC_HANDLER DecModeSet,
                         PTR_ESC_HANDLER DecModeReset,
                         PTR_ESC_HANDLER GeneralOscHandler,
                         PTR_ESC_HANDLER RequestResValuesHandler,
                         PTR_ESC_HANDLER RequestTerminfoHandler,
                         PTR_ESC_HANDLER SetCursorStyleHandler

  };

  void ReportUnhandledSequence();
  void ReportUnparsedSequence();

  void MakeBuffer();
  void SetIndexedColor(u32 fg_or_bg, u32 color);
  void SetRgbColor(u32 fg_or_bg, u32 r, u32 g, u32 b);
  void SetMode(u32 mode, bool flag);
  void ChangeModeSettings(const std::vector<Token>& tokens, bool flag);

  void SetPrivateMode(u32 mode, bool flag);
  void HandleIndividualModifierForMSequence(u32 mod);

  Terminal(const std::string& shell_path);
  Terminal(const Terminal& term) = delete;
  void operator=(const Terminal& term) = delete;

  inline void AssignId(int id) { id_ = id; }

  void ResetFgColor();

  void ResetBgColor();

  void SetReverseWraparound(bool flag);

  void SetAutowrap(bool flag);

  bool IsReverseWraparoundEnabled();

  bool IsAutowrapEnabled();

  void SetLNM(bool flag);

  void SetCellFlags(CellFlags flags);
  void ResetCellFlags(CellFlags flags);
  void ToggleCellFlags(CellFlags flags);

  void SetFgColor(Color col);

  void SetBgColor(Color col);

  void SetCursor(u32 x, u32 y);

  void SetCursorX(u32 x);

  void SetCursorY(u32 y);

  void SetCursorVisibility(bool flag);

  void ReverseIndex();
  void GoForwardX();
  void GoBackX();

  void CarriageReturn();
  void LineFeed();

  bool Set(ColoredCell chr);

  bool Set(Cell chr);

  bool Set(u32 x, u32 y, Cell chr);

  void SwitchToAlternateBuffer();
  void SwitchToNormalBuffer();
  bool IsUsingNormalBuffer();

  void ReportMouseEvent(u32 btn, bool is_down, bool is_motion, u32 mods, u32 x,
                        u32 y);

  inline ColoredCell GetEmptyCell() {
    return ColoredCell(Cell(' ', 0), current_fg_, current_bg_);
  }

  inline ColoredCell GetDefaultEmptyCell() {
    return ColoredCell(Cell(' ', 0), default_fg_, default_bg_);
  }

  Rect<u32> GetDefaultScrollArea();

 public:
  ~Terminal();

  void SetWindowSize(u32 width, u32 height);
  inline int Id() const { return id_; }
  static int Create(const std::string& shell_path);
  static std::optional<std::shared_ptr<Terminal>> Get(int id);

  inline std::shared_ptr<CellBuffer> CurrentBuffer() { return buf_; }

  bool IsLNMSet();

  u32 CursorX() const;
  u32 CursorY() const;

  bool IsCursorVisible();

  void InterpretPtyInput(char byte);

  bool TryScrollBufferUp(u32 pixels);
  bool TryScrollBufferDown(u32 pixels);
  bool TryResetUserScroll();
  bool IsUserScrolledUp();

  void HandleMouseScroll(const EventMouseScroll& event);
  void HandleMousePos(const EventMousePos& event);
  void HandleMouseButton(const EventMouseButton& event);

  void WriteToPty(std::vector<char>&& bytes);
};
}  // namespace bitty

#endif /* __BITTY_TERMINAL_HH__ */