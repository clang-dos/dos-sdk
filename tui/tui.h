/* <tui.h>: a text-mode window toolkit for 16-bit DOS, in C++.
 *
 * Overlapping windows on a desktop, a menu bar with pull-down menus, a
 * status line, modal dialogs with buttons, input lines, check boxes,
 * radio buttons and list boxes, and scrolling text windows, driven by the
 * keyboard and the mouse (INT 33h), in the style of the DOS application
 * frameworks of the early 1990s.
 *
 * A program makes an Application, gives it menus and a status line, and
 * calls run(). Views draw into a screen buffer that is copied to video
 * memory after each event; a view handles events in handleEvent and
 * passes on what it does not use. Commands (cmXXX) carry the choices of
 * menus, buttons and the status line to the views that act on them.
 *
 *     class MyApp : public tui::Application {
 *         void handleEvent(tui::Event &e) override { ... }
 *     };
 *
 * Link with -ltui (clang++, which links the C++ runtime).
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef _TUI_H_
#define _TUI_H_

namespace tui {

/* ---- Keys: (scan code << 8) | character, as INT 16h returns them. ---- */
enum : unsigned {
    kbEsc = 0x011b, kbEnter = 0x1c0d, kbTab = 0x0f09, kbShiftTab = 0x0f00,
    kbBack = 0x0e08, kbSpace = 0x3920,
    kbUp = 0x4800, kbDown = 0x5000, kbLeft = 0x4b00, kbRight = 0x4d00,
    kbHome = 0x4700, kbEnd = 0x4f00, kbPgUp = 0x4900, kbPgDn = 0x5100,
    kbIns = 0x5200, kbDel = 0x5300,
    kbF1 = 0x3b00, kbF2 = 0x3c00, kbF3 = 0x3d00, kbF4 = 0x3e00, kbF5 = 0x3f00,
    kbF6 = 0x4000, kbF7 = 0x4100, kbF8 = 0x4200, kbF9 = 0x4300, kbF10 = 0x4400,
    kbAltF3 = 0x6a00, kbCtrlF5 = 0x6200,
    kbAltA = 0x1e00, kbAltB = 0x3000, kbAltC = 0x2e00, kbAltD = 0x2000,
    kbAltE = 0x1200, kbAltF = 0x2100, kbAltG = 0x2200, kbAltH = 0x2300,
    kbAltI = 0x1700, kbAltJ = 0x2400, kbAltK = 0x2500, kbAltL = 0x2600,
    kbAltM = 0x3200, kbAltN = 0x3100, kbAltO = 0x1800, kbAltP = 0x1900,
    kbAltQ = 0x1000, kbAltR = 0x1300, kbAltS = 0x1f00, kbAltT = 0x1400,
    kbAltU = 0x1600, kbAltV = 0x2f00, kbAltW = 0x1100, kbAltX = 0x2d00,
    kbAltY = 0x1500, kbAltZ = 0x2c00,
};

/* The Alt key code of a letter, or 0. */
unsigned altKey(char c);

/* ---- Commands. Programs number their own from cmUser. ---- */
enum : unsigned {
    cmNone = 0,
    cmQuit = 1,    /* end Application::run */
    cmClose = 2,   /* close the focused window */
    cmZoom = 3,    /* zoom the focused window */
    cmNext = 4,    /* focus the next window */
    cmPrev = 5,
    cmTile = 6,    /* arrange the windows side by side */
    cmCascade = 7, /* arrange them overlapping */
    cmMenu = 8,    /* open the menu bar */
    cmOK = 10,     /* dialog results */
    cmCancel = 11,
    cmYes = 12,
    cmNo = 13,
    cmDefault = 14, /* broadcast: press the default button */
    cmListSelect = 15, /* a list box item was double-clicked or Entered */
    cmUser = 100,
};

/* ---- Events. ---- */
enum : unsigned {
    evNothing = 0,
    evKeyDown = 0x01,
    evMouseDown = 0x02,
    evMouseUp = 0x04,
    evMouseMove = 0x08,
    evCommand = 0x10,
    evBroadcast = 0x20,
    evMouse = evMouseDown | evMouseUp | evMouseMove,
};

struct Event {
    unsigned what;
    unsigned key;          /* evKeyDown */
    unsigned char shift;   /* shift keys (INT 16h function 2) */
    int x, y;              /* mouse, in screen cells */
    unsigned char buttons; /* 1 left, 2 right */
    bool dbl;              /* a double click */
    unsigned command;      /* evCommand, evBroadcast */
    void *info;
    char ch() const { return (char)(key & 0xff); }
};

/* ---- Colors: background * 16 + foreground, as in text mode. ---- */
namespace color {
enum : unsigned char {
    Desktop = 0x71,
    MenuText = 0x70, MenuHot = 0x74, MenuSel = 0x20, MenuSelHot = 0x2e, MenuDisabled = 0x78,
    WinFrame = 0x1f, WinFrameIdle = 0x17, WinText = 0x1e, WinIcon = 0x1a,
    DlgFrame = 0x7f, DlgFrameIdle = 0x78, DlgText = 0x70, DlgHot = 0x7e,
    Button = 0x20, ButtonHot = 0x2e, ButtonFocus = 0x2f, ButtonDefault = 0x2b, ButtonShadow = 0x70,
    Input = 0x1f, InputArrow = 0x1a, InputSel = 0x2f,
    Cluster = 0x30, ClusterHot = 0x3e, ClusterFocus = 0x3f,
    List = 0x30, ListFocus = 0x2f, ListSel = 0x3e, ScrollBar = 0x31,
    Label = 0x70, LabelHot = 0x7e, LabelFocus = 0x7f,
    Shadow = 0x08,
};
}

struct Rect {
    int x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

class Group;

/* Drawing into the screen buffer: coordinates relative to an origin, and
 * nothing outside the clip rectangle (absolute). */
class DrawContext {
public:
    int ox, oy;
    Rect clip;
    DrawContext(int ox_, int oy_, const Rect &clip_) : ox(ox_), oy(oy_), clip(clip_) {}
    void put(int x, int y, unsigned char ch, unsigned char attr) const;
    void fill(int x, int y, int w, int h, unsigned char ch, unsigned char attr) const;
    /* Text; with hot != 0, a ~ turns the hot color on and off. Returns
     * the width drawn. */
    int text(int x, int y, const char *s, unsigned char attr, unsigned char hot = 0) const;
    /* A frame of single or double lines. */
    void box(int x, int y, int w, int h, unsigned char attr, bool dbl) const;
    /* Darken the cells, keeping their characters. */
    void shade(int x, int y, int w, int h) const;
    /* A context for a part of this one. */
    DrawContext sub(const Rect &r) const;
};

/* The width of s without its ~ marks, and its hot letter (upper case). */
int cstrlen(const char *s);
char hotChar(const char *s);

/* ---- Views. ---- */
enum : unsigned {
    ofSelectable = 0x01, /* can have the focus */
    ofTopSelect = 0x02,  /* comes to the front when selected (windows) */
    ofPreProcess = 0x04, /* sees keys before the focused view */
    ofPostProcess = 0x08,/* sees keys the focused view left */
    ofShadow = 0x10,     /* casts a shadow */
    ofCentered = 0x20,   /* centered in its owner when inserted */
};

/* How a view follows when its owner changes size: which of its edges
 * move with the owner's right and bottom edges. */
enum : unsigned char {
    gfGrowLoX = 1, gfGrowLoY = 2, gfGrowHiX = 4, gfGrowHiY = 8,
    gfGrowAll = 15,  /* moves with the bottom right corner */
    gfGrowSize = gfGrowHiX | gfGrowHiY, /* stretches */
};

class View {
public:
    explicit View(const Rect &bounds);
    virtual ~View();
    Rect bounds;          /* in the owner's client coordinates */
    Group *owner;
    View *next;           /* the next view up in the owner's list */
    unsigned options;
    bool visible;
    unsigned helpCtx;
    unsigned char growMode;

    /* Move or resize the view (children follow their growMode). */
    virtual void setBounds(const Rect &r);

    virtual void draw(const DrawContext &dc);
    virtual void handleEvent(Event &e);
    /* Called when the view gains (true) or loses the focus. */
    virtual void focusChanged(bool focused);
    /* Can the focus leave? A dialog control may check its data here. */
    virtual bool valid(unsigned command);

    void select();
    bool focused() const;
    /* The screen position of the view's top left corner. */
    int absX() const;
    int absY() const;
    bool mouseIn(const Event &e) const;
    /* Mark the event as handled. */
    static void clearEvent(Event &e) { e.what = evNothing; }
};

class Group : public View {
public:
    explicit Group(const Rect &bounds);
    ~Group() override;
    View *first;    /* the bottom view */
    View *current;  /* the focused view */
    unsigned endState; /* nonzero ends execView */

    /* Insert v on top, and select it if it can be selected. */
    void insert(View *v);
    /* Take v out (not deleting it). */
    void remove(View *v);
    void toFront(View *v);
    void setCurrent(View *v);
    /* Focus the next (or previous) selectable view. */
    void selectNext(bool backward);
    /* Where the children are, within the bounds: all of them, or inside a
     * frame for a window. */
    virtual Rect client() const;
    void setBounds(const Rect &r) override;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void focusChanged(bool focused) override;
    /* The topmost child under (x, y), in screen cells. */
    View *childAt(int x, int y) const;
    void endModal(unsigned command) { endState = command; }
    template <class F> void forEach(F f) {
        for (View *v = first; v;) {
            View *n = v->next;
            f(v);
            v = n;
        }
    }
};

/* ---- Windows. ---- */
enum : unsigned {
    wfMove = 1, wfGrow = 2, wfClose = 4, wfZoom = 8,
    wfDefault = wfMove | wfGrow | wfClose | wfZoom,
};

class Window : public Group {
public:
    Window(const Rect &bounds, const char *title, int number = 0);
    ~Window() override;
    const char *title;
    int number;     /* shown in the frame, and Alt+number focuses it */
    unsigned flags;
    Rect zoomRect;  /* the bounds before zooming */
    bool dialogColors;
    int minW, minH;

    Rect client() const override;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    virtual void close();
    void zoom();
    unsigned char frameColor() const;
    unsigned char textColor() const;
private:
    void dragMove(Event &e);
    void dragSize(Event &e);
};

class Desktop : public Group {
public:
    explicit Desktop(const Rect &bounds);
    unsigned char pattern;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void tile();
    void cascade();
};

/* ---- Menus. ---- */
struct MenuItem {
    const char *name;     /* with ~ around the hot letter; 0: a separator */
    unsigned command;
    unsigned key;         /* a shortcut key, or 0 */
    const char *keyName;  /* how the shortcut is shown */
    struct Menu *submenu;
    bool disabled;
    MenuItem *next;
};

struct Menu {
    const char *name;     /* the title on the menu bar */
    MenuItem *items;
    Menu *next;
    /* Add an item, a separator or a submenu; return this menu, so calls
     * chain: m.item(...).item(...). */
    Menu &item(const char *name, unsigned command, unsigned key = 0, const char *keyName = 0);
    Menu &separator();
    Menu &submenu(const char *name, Menu *sub);
    Menu();
};

class MenuBar : public View {
public:
    explicit MenuBar(const Rect &bounds);
    ~MenuBar() override;
    Menu *menus;
    /* Add a pull-down menu titled name. */
    Menu &add(const char *name);
    /* Enable or disable every item with this command. */
    void enable(unsigned command, bool on);
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    /* Run the menus from menu number index; returns the chosen command. */
    unsigned execute(int index, Event *start);
    int open;   /* the pulled-down menu, or -1 */
    /* Where menu index starts on the bar, and the menu at x (or -1). */
    int menuX(int index) const;
    int menuAt(int x) const;
};

struct StatusItem {
    const char *text;   /* "~Alt-X~ Exit"; 0: a key with nothing shown */
    unsigned key;
    unsigned command;
    StatusItem *next;
};

class StatusLine : public View {
public:
    explicit StatusLine(const Rect &bounds);
    ~StatusLine() override;
    StatusItem *items;
    const char *hint;   /* shown at the right */
    StatusLine &add(const char *text, unsigned key, unsigned command);
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
};

/* ---- Dialogs and controls. ---- */
class Dialog : public Window {
public:
    Dialog(const Rect &bounds, const char *title);
    void handleEvent(Event &e) override;
    void close() override;
};

class StaticText : public View {
public:
    StaticText(const Rect &bounds, const char *text);
    const char *text;   /* words wrap; \n breaks; a line starting \003 is centered */
    void draw(const DrawContext &dc) override;
};

class Label : public View {
public:
    Label(const Rect &bounds, const char *text, View *link);
    const char *text;
    View *link;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
};

class Button : public View {
public:
    Button(const Rect &bounds, const char *title, unsigned command, bool isDefault = false);
    const char *title;
    unsigned command;
    bool isDefault;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void press();
private:
    bool down;
};

class InputLine : public View {
public:
    InputLine(const Rect &bounds, int maxLen);
    ~InputLine() override;
    char *data;
    int maxLen;
    void setText(const char *s);
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void focusChanged(bool focused) override;
private:
    int pos, first;
    bool selAll;
};

/* Check boxes and radio buttons: a column of items, from a list of
 * strings ended by 0. */
class Cluster : public View {
public:
    Cluster(const Rect &bounds, const char *const *items);
    const char *const *items;
    int count;
    unsigned value;  /* check boxes: a bit per item; radio buttons: the item */
    int sel;
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void focusChanged(bool focused) override;
protected:
    virtual bool marked(int i) const = 0;
    virtual void press(int i) = 0;
    virtual const char *icon() const = 0;  /* "[ ]" or "( )" */
    virtual char mark() const = 0;
};

class CheckBoxes : public Cluster {
public:
    CheckBoxes(const Rect &bounds, const char *const *items) : Cluster(bounds, items) {}
    bool checked(int i) const { return (value >> i) & 1; }
protected:
    bool marked(int i) const override { return checked(i); }
    void press(int i) override { value ^= 1u << i; }
    const char *icon() const override { return "[ ]"; }
    char mark() const override { return 'X'; }
};

class RadioButtons : public Cluster {
public:
    RadioButtons(const Rect &bounds, const char *const *items) : Cluster(bounds, items) {}
protected:
    bool marked(int i) const override { return (int)value == i; }
    void press(int i) override { value = (unsigned)i; }
    const char *icon() const override { return "( )"; }
    char mark() const override { return '\x07'; }
};

/* A list of strings with a scroll bar in its last column. */
class ListBox : public View {
public:
    ListBox(const Rect &bounds, const char *const *items, int count);
    const char *const *items;
    int count;
    int sel, top;
    void setItems(const char *const *items, int count);
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    void focusItem(int i);
};

/* A window's worth of text lines, scrolled with the keys, the mouse and
 * a scroll bar. Lines are copied in. */
class TextView : public View {
public:
    explicit TextView(const Rect &bounds);
    ~TextView() override;
    void addLine(const char *s);
    void addText(const char *s);   /* split at \n */
    void clear();
    int lineCount() const { return nlines; }
    void draw(const DrawContext &dc) override;
    void handleEvent(Event &e) override;
    unsigned char textAttr;
private:
    char **lines;
    int nlines, cap, top, left;
    int width() const;
};

/* ---- The application. ---- */
class Application : public Group {
public:
    Application();
    ~Application() override;
    MenuBar *menuBar;
    StatusLine *statusLine;
    Desktop *desktop;

    /* Run until cmQuit. Returns 0. */
    int run();
    /* Insert v into the desktop and run it modally until it ends with a
     * command, which is returned. v is taken out again (not deleted). */
    unsigned execView(Group *v);
    /* The next event: a queued one, a key or the mouse. Waits. */
    void getEvent(Event &e);
    /* Queue an event for getEvent. */
    void putEvent(const Event &e);
    void putCommand(unsigned command, void *info = 0);
    /* Draw everything and show it. */
    void redraw();
    /* Called while waiting for an event. */
    virtual void idle();
    void handleEvent(Event &e) override;
    /* Add a window to the desktop (cascaded) and focus it. */
    void addWindow(Window *w);
    /* Delete a view at the next safe moment (after the current event). */
    void destroy(View *v);
    int cols() const;
    int rows() const;
    bool hasMouse() const;
    Group *modal;       /* the view getting the events */
private:
    void route(Event &e);
};

/* The application. */
extern Application *app;

/* ---- Ready-made dialogs. ---- */
enum : unsigned {
    mfWarning = 0x0000, mfError = 0x0001, mfInformation = 0x0002, mfConfirmation = 0x0003,
    mfYesButton = 0x0100, mfNoButton = 0x0200, mfOKButton = 0x0400, mfCancelButton = 0x0800,
    mfYesNoCancel = mfYesButton | mfNoButton | mfCancelButton,
    mfOKCancel = mfOKButton | mfCancelButton,
};

/* A message and buttons; returns cmOK, cmCancel, cmYes or cmNo. */
unsigned messageBox(const char *msg, unsigned flags);
/* Ask for a line of text into buf (size bytes, kept on cancel). */
unsigned inputBox(const char *title, const char *label, char *buf, int size);

} // namespace tui

#endif /* _TUI_H_ */
