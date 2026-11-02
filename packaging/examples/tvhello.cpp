// Turbo Vision on DOS: a menu bar, a status line, a dialog with an input
// line and a message box.
//
//   clang++ --target=i8086-pc-msdos -mcmodel=large -Os tvhello.cpp \
//     -ltvision -o tvhello.exe
//
// Turbo Vision needs a memory model with far code: medium, large, huge or
// huge16.

#define Uses_TApplication
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TLabel
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TRect
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_MsgBox
#include <tvision/tv.h>

#include <stdio.h>

const ushort cmGreet = 100;

class THelloApp : public TApplication
{
public:
    THelloApp() :
        TProgInit(&THelloApp::initStatusLine, &THelloApp::initMenuBar,
                  &THelloApp::initDeskTop)
    {
    }

    void handleEvent(TEvent &event) override
    {
        TApplication::handleEvent(event);
        if (event.what == evCommand && event.message.command == cmGreet)
        {
            greet();
            clearEvent(event);
        }
    }

    static TMenuBar *initMenuBar(TRect r)
    {
        r.b.y = r.a.y + 1;
        return new TMenuBar(r,
            *new TSubMenu("~H~ello", kbAltH) +
                *new TMenuItem("~G~reeting...", cmGreet, kbF2, hcNoContext, "F2") +
                newLine() +
                *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X"));
    }

    static TStatusLine *initStatusLine(TRect r)
    {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~F2~ Greeting", kbF2, cmGreet) +
                *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
                *new TStatusItem(0, kbF10, cmMenu));
    }

private:
    void greet()
    {
        TDialog *d = new TDialog(TRect(20, 6, 60, 15), "Hello");
        TInputLine *name = new TInputLine(TRect(3, 3, 37, 4), 30);
        d->insert(name);
        d->insert(new TLabel(TRect(2, 2, 22, 3), "Your ~n~ame:", name));
        d->insert(new TButton(TRect(8, 6, 18, 8), "O~K~", cmOK, bfDefault));
        d->insert(new TButton(TRect(21, 6, 31, 8), "Cancel", cmCancel, bfNormal));
        d->selectNext(False);

        char text[31] = "";
        d->setData(text);
        if (deskTop->execView(d) == cmOK)
        {
            d->getData(text);
            char msg[64];
            snprintf(msg, sizeof msg, "\003Hello, %s!", text[0] ? text : "world");
            messageBox(msg, mfInformation | mfOKButton);
        }
        destroy(d);
    }
};

int main()
{
    THelloApp app;
    app.run();
    return 0;
}
