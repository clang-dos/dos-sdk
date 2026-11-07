/* A tour of <tui.h>: menus, a status line, windows to move, resize, zoom,
 * tile and cascade, and dialogs with every kind of control. Works with
 * the keyboard alone (F10 or Alt+letter for the menus, F6 between
 * windows, Tab between controls) and with a mouse.
 *
 * Build: clang++ --target=i8086-pc-msdos -Os tuidemo.cpp -ltui -o tuidemo.exe
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdio.h>
#include <string.h>
#include <tui.h>

using namespace tui;

enum {
    cmAbout = cmUser,
    cmNewText,
    cmOpen,
    cmOptions,
    cmMessage,
    cmAskName,
    cmColors,
};

static const char *const languages[] = {
    "Assembly", "BASIC", "C", "C++", "COBOL", "Forth", "Fortran", "Lisp",
    "Logo", "Modula-2", "Pascal", "Prolog", "Smalltalk",
};

static char name[40] = "World";

/* An item's text without its ~ marks. */
static const char *
plain(const char *s)
{
    static char buf[40];
    int n = 0;
    for (; *s && n < 39; s++)
        if (*s != '~')
            buf[n++] = *s;
    buf[n] = 0;
    return buf;
}

class Demo : public Application {
public:
    Demo();
    void handleEvent(Event &e) override;
private:
    int windows;
    void newText();
    void options();
    void about();
    void openFile();
};

Demo::Demo() : windows(0)
{
    Menu &file = menuBar->add("~F~ile");
    file.item("~N~ew text window", cmNewText, kbF4, "F4")
        .item("~O~pen file...", cmOpen, kbF3, "F3")
        .separator()
        .item("E~x~it", cmQuit, kbAltX, "Alt-X");
    Menu *arrange = new Menu;
    arrange->item("~T~ile", cmTile).item("C~a~scade", cmCascade);
    Menu &win = menuBar->add("~W~indow");
    win.item("~Z~oom", cmZoom, kbF5, "F5")
        .item("~N~ext", cmNext, kbF6, "F6")
        .item("~C~lose", cmClose, kbAltF3, "Alt-F3")
        .separator()
        .submenu("~A~rrange", arrange);
    Menu &dlg = menuBar->add("~D~ialogs");
    dlg.item("~O~ptions...", cmOptions, kbF2, "F2")
        .item("~M~essage box...", cmMessage)
        .item("~I~nput box...", cmAskName);
    menuBar->add("~H~elp").item("~A~bout...", cmAbout, kbF1, "F1");

    statusLine->add("~F1~ About", kbF1, cmAbout)
        .add("~F2~ Options", kbF2, cmOptions)
        .add("~F4~ New", kbF4, cmNewText)
        .add("~F10~ Menu", kbF10, cmMenu)
        .add("~Alt-X~ Exit", kbAltX, cmQuit);
    statusLine->hint = "clang-dos tui";

    newText();
    about();
}

void
Demo::newText()
{
    char title[32];
    snprintf(title, sizeof title, "Text %d", ++windows);
    Window *w = new Window(Rect(0, 0, 44, 13), strdup(title), windows < 10 ? windows : 0);
    TextView *t = new TextView(Rect(0, 0, 42, 11));
    t->addText("Windows can be moved by their title bar,\n"
               "resized from the bottom right corner, and\n"
               "zoomed with the box at the top right or a\n"
               "double click on the title.\n"
               "\n"
               "Keys: F6 next window, F5 zoom, Alt-F3\n"
               "close, F10 menu, Alt+letter a menu.\n");
    for (int i = 1; i <= 40; i++) {
        char line[48];
        snprintf(line, sizeof line, "Line %2d of the text in window %d", i, windows);
        t->addLine(line);
    }
    w->insert(t);
    addWindow(w);
}

void
Demo::about()
{
    Dialog *d = new Dialog(Rect(0, 0, 40, 11), "About");
    d->options |= ofCentered;
    d->insert(new StaticText(Rect(1, 1, 36, 4),
                             "\003tui for clang-dos\n\003\n"
                             "\003Windows, menus and dialogs\n\003in 16-bit DOS text mode"));
    d->insert(new Button(Rect(14, 6, 10, 2), "O~K~", cmOK, true));
    execView(d);
    delete d;
}

void
Demo::options()
{
    static const char *const checks[] = { "~S~ound", "~M~ouse", "~A~uto-save", 0 };
    static const char *const radios[] = { "~F~ast", "~B~alanced", "~T~horough", 0 };
    Dialog *d = new Dialog(Rect(0, 0, 58, 18), "Options");
    d->options |= ofCentered;
    InputLine *in = new InputLine(Rect(13, 1, 20, 1), 39);
    in->setText(name);
    d->insert(new Label(Rect(2, 1, 10, 1), "~N~ame:", in));
    d->insert(in);
    CheckBoxes *cb = new CheckBoxes(Rect(3, 4, 20, 3), checks);
    cb->value = 3;
    d->insert(new Label(Rect(2, 3, 12, 1), "~O~ptions", cb));
    d->insert(cb);
    RadioButtons *rb = new RadioButtons(Rect(3, 9, 20, 3), radios);
    rb->value = 1;
    d->insert(new Label(Rect(2, 8, 12, 1), "~S~peed", rb));
    d->insert(rb);
    ListBox *lb = new ListBox(Rect(36, 2, 18, 10), languages, sizeof languages / sizeof *languages);
    lb->focusItem(2);
    d->insert(new Label(Rect(35, 1, 18, 1), "~L~anguage", lb));
    d->insert(lb);
    d->insert(new Button(Rect(14, 13, 12, 2), "O~K~", cmOK, true));
    d->insert(new Button(Rect(30, 13, 12, 2), "Cancel", cmCancel));
    in->select();
    if (execView(d) == cmOK) {
        strncpy(name, in->data, sizeof name - 1);
        char msg[160];
        snprintf(msg, sizeof msg, "Name: %s\nOptions: %s%s%s\nSpeed: %s\nLanguage: %s",
                 name, cb->checked(0) ? "sound " : "", cb->checked(1) ? "mouse " : "",
                 cb->checked(2) ? "auto-save" : "", plain(radios[rb->value]),
                 languages[lb->sel]);
        delete d;
        messageBox(msg, mfInformation | mfOKButton);
        return;
    }
    delete d;
}

void
Demo::openFile()
{
    char path[64] = "README.TXT";
    if (inputBox("Open file", "~F~ile name:", path, sizeof path) != cmOK)
        return;
    FILE *f = fopen(path, "r");
    if (!f) {
        messageBox("The file could not be opened.", mfError | mfOKButton);
        return;
    }
    Window *w = new Window(Rect(0, 0, 60, 16), strdup(path));
    TextView *t = new TextView(Rect(0, 0, 58, 14));
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        t->addText(line[0] ? line : " ");
    }
    fclose(f);
    w->insert(t);
    addWindow(w);
}

void
Demo::handleEvent(Event &e)
{
    Application::handleEvent(e);
    if (e.what != evCommand)
        return;
    switch (e.command) {
    case cmAbout:
        about();
        break;
    case cmNewText:
        newText();
        break;
    case cmOpen:
        openFile();
        break;
    case cmOptions:
        options();
        break;
    case cmMessage:
        if (messageBox("Save the changes to the document before closing?",
                       mfConfirmation | mfYesNoCancel) == cmYes)
            messageBox("Saved.", mfInformation | mfOKButton);
        break;
    case cmAskName: {
        char msg[64];
        if (inputBox("Your name", "~N~ame:", name, sizeof name) == cmOK) {
            snprintf(msg, sizeof msg, "Hello, %s!", name);
            messageBox(msg, mfInformation | mfOKButton);
        }
        break;
    }
    case cmTile:
    case cmCascade:
    case cmNext:
    case cmPrev:
        desktop->handleEvent(e);
        break;
    default:
        return;
    }
    clearEvent(e);
}

int
main()
{
    Demo demo;
    return demo.run();
}
