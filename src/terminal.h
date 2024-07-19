#ifndef TERMINAL
#define TERMINAL
#define CTRL_KEY(k) (k & 0x1f)
#define SHIFT_KEY(k) ((k) & 0x20)
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

int editorReadKey();

void disablerawMode();

void enabelRawMode();
#endif // !TERMINAL
