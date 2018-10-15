#include <unordered_map>

#include "portotop.hpp"
#include "version.hpp"

static double ParseNumber(const std::string &str) {
    return strtod(str.c_str(), nullptr);
}

static double ParseValue(const std::string &value, bool map) {
    if (!map)
        return ParseNumber(value);

    double ret = 0;
    TUintMap tmp;
    if (!StringToUintMap(value, tmp)) {
        for (auto it: tmp)
            ret += it.second;
    }
    return ret;
}

static double DfDt(double curr, double prev, uint64_t dt) {
    if (dt)
        return 1000.0 * (curr - prev) / dt;
    return 0;
}

static double PartOf(double value, double total) {
    return value / total;
}
////////////////////////////////////////////////////////////////////////////////

int TConsoleScreen::Width() {
    return getmaxx(Wnd);
}
int TConsoleScreen::Height() {
    return getmaxy(Wnd);
}

TConsoleScreen::TConsoleScreen() {
    Wnd = initscr();
    start_color();

    init_pair(1, COLOR_BLACK, COLOR_RED);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);
    init_pair(3, COLOR_BLACK, COLOR_BLUE);
    init_pair(4, COLOR_BLACK, COLOR_YELLOW);
    init_pair(5, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(6, COLOR_BLACK, COLOR_CYAN);

    clear();
    cbreak();
    noecho();
    intrflush(stdscr, true);
    keypad(stdscr, true);
    curs_set(0);
}
TConsoleScreen::~TConsoleScreen() {
    endwin();
}
void TConsoleScreen::SetTimeout(int ms) {
    timeout(ms);
}
template<class T>
void TConsoleScreen::PrintAt(T arg, int x, int y, int width, bool leftaligned, int attr) {
    PrintAt(std::to_string(arg), x, y, width, leftaligned, attr);
}
void TConsoleScreen::PrintAt(std::string str0, int x0, int y0, int w0, bool leftaligned,
             int attr) {
    if (x0 + w0 < 0 || x0 >= Width())
        return;

    int x = x0 < 0 ? 0 : x0;
    int w = w0 - (x - x0);
    if (x + w >= Width())
        w = Width() - x;

    std::string str;
    if ((int)str0.length() > x - x0)
        str = str0.substr(x - x0, w);
    else
        str = std::string(w, ' ');

    if (attr)
        attron(attr);
    mvprintw(y0, x, (leftaligned ? "%-*s" : "%*s"), w, str.c_str());
    if (attr)
        attroff(attr);
}
void TConsoleScreen::Refresh() {
    refresh();
}
void TConsoleScreen::Erase() {
    erase();
}
void TConsoleScreen::Clear() {
    clear();
}
int TConsoleScreen::Getch() {
    return wgetch(Wnd);
}
void TConsoleScreen::Save() {
    def_prog_mode();
    endwin();
}
void TConsoleScreen::Restore() {
    pid_t pid = getpgrp();

    if (pid >= 0)
        tcsetpgrp(1, pid);

    reset_prog_mode();
    refresh();
}
int TConsoleScreen::Dialog(std::string text, const std::vector<std::string> &buttons) {
    int selected = 0;
    bool done = false;

    int x0 = Width() / 2 - text.length() / 2;
    int y0 = Height() / 2 - 3;

    int w = 0;
    for (auto &b : buttons)
        w += b.length() + 1;
    int x00 = Width() / 2 - w / 2;

    WINDOW *win = newwin(5, std::max((int)text.length(), w) + 4, y0 - 1, std::min(x0, x00) - 2);
    box(win, 0, 0);
    wrefresh(win);

    while (!done) {
        PrintAt(text, x0, y0, text.length(), false);

        int x = x00;
        int n = 0;
        for (auto &b : buttons) {
            PrintAt(b, x, y0 + 2, b.length(), false, selected == n ? A_REVERSE : 0);
            x += 1 + b.length();
            n++;
        }

        switch(Getch()) {
        case KEY_LEFT:
            if (--selected < 0)
                selected = 0;
            break;
        case KEY_RIGHT:
            if ((unsigned long) ++selected > buttons.size() - 1)
                selected = buttons.size() - 1;
            break;
        case '\n':
            done = true;
            break;
        }

        Refresh();
    }

    delwin(win);

    return selected;
}
void TConsoleScreen::ErrorDialog(Porto::TPortoApi &api) {
    Dialog(api.GetLastError(), {"Ok"});
}
void TConsoleScreen::InfoDialog(std::vector<std::string> lines) {
    unsigned int w = 0;
    unsigned int h = lines.size();
    for (auto &l : lines)
        if (l.length() > w)
            w = l.length();
    int x0 = Width() / 2 - w / 2;
    int y0 = Height() / 2 - h / 2;
    bool done = false;

    WINDOW *win = newwin(h + 2, w + 4, y0 - 1, x0 - 2);
    box(win, 0, 0);
    wrefresh(win);

    while (!done) {
        int n = 0;
        for (auto &l : lines) {
            PrintAt(l, x0, y0 + n, l.length(), false);
            n++;
        }

        switch(Getch()) {
        case 0:
        case -1:
            break;
        default:
            done = true;
            break;
        }

        Refresh();
    }

    delwin(win);
}

void TConsoleScreen::HelpDialog() {
    std::vector<std::string> help =
        {std::string("portoctl top ") + PORTO_VERSION + " " + PORTO_REVISION,
         "",
         "left, right, home, end - select column/scroll",
         "up, down, page up, page down - select container/scroll",
         "<, > - horizontal scroll without changing selection",
         "tab - expand containers tree: first, second, all",
         "s, enter - sort/invert selected column",
         "@ - go to self container",
         "! - mark selected container",
         "",
         "1-9,0 - set update delay to 1s-9s and 10s",
         "space - pause/resume screen updates",
         "u - update screen",
         "",
         "d, del - disable column",
         "backspace - move column left",
         "f - choose columns",
         "a - show all",
         "c - show cpu",
         "m - show memory",
         "n - show network",
         "i - show disk io",
         "p - show policy and porto",
         "",
         "g - get properties",
         "o - show stdout",
         "e - show stderr",
         "t - run top in container",
         "b - run bash in container",
         "",
         "S - start/stop container",
         "P - pause/resume container",
         "K - kill container",
         "D - destroy container",
         "",
         "q - quit",
         "h,? - help"};
    InfoDialog(help);
}

void TConsoleScreen::ColumnsMenu(std::vector<TColumn> &columns) {
    const int MENU_SPACING = 2;

    const char CHECKED[] = " [*]  ";
    const char UNCHECKED[] = " [ ]  ";
    const int CHECKBOX_SIZE = strlen(CHECKED);

    const int BOX_BORDER = 2;


    int title_width = 0, desc_width = 0;

    for (auto &col : columns) {
        title_width = std::max(title_width, (int)col.Title.length());
        desc_width = std::max(desc_width, (int)col.Description.length());
    }

    int menu_width = title_width + desc_width + MENU_SPACING;
    int win_width = menu_width + BOX_BORDER + CHECKBOX_SIZE + MENU_SPACING;

    const int menu_lines = std::min((int)columns.size(),
                                    std::max(1, Height() - 6));

    const int win_height = menu_lines + BOX_BORDER + 2 + 1;

    int x0 = Width() / 2 - win_width / 2;
    int y0 = Height() / 2 - win_height / 2;

    WINDOW *win = newwin(win_height, win_width, y0, x0);

    box(win, 0, 0);
    wrefresh(win);

    std::vector<ITEM *> items;

    for (auto &col : columns) {
        auto item = new_item(col.Title.c_str(), col.Description.c_str());
        items.push_back(item);
    }

    items.push_back(NULL);

    mvwprintw(win, 1, 2, "Select displayed columns:");

    MENU *menu = new_menu(items.data());
    WINDOW *sub = derwin(win, menu_lines, menu_width, 3, BOX_BORDER / 2 + CHECKBOX_SIZE);

    set_menu_win(menu, win);
    set_menu_sub(menu, sub);
    set_menu_mark(menu, "");
    set_menu_format(menu, menu_lines, 1);
    set_menu_spacing(menu, MENU_SPACING, 0, 0);

    post_menu(menu);

    bool done = false;

    while (!done) {
        for (int i = 0; i < menu_lines; i++) {
            bool hidden = columns[top_row(menu) + i].Hidden;
            mvwprintw(win, 3 + i, 1, hidden ? UNCHECKED : CHECKED);
        }

        wrefresh(win);

        switch(Getch()) {
            case KEY_DOWN:
                menu_driver(menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(menu, REQ_UP_ITEM);
                break;
            case KEY_NPAGE:
                menu_driver(menu, REQ_SCR_DPAGE);
                break;
            case KEY_PPAGE:
                menu_driver(menu, REQ_SCR_UPAGE);
                break;
            case KEY_HOME:
                menu_driver(menu, REQ_FIRST_ITEM);
                break;
            case KEY_END:
                menu_driver(menu, REQ_LAST_ITEM);
                break;
            case 'f':
            case 'q':
            case 'Q':
            case '\n':
                done = true;
                break;
            case ' ':
                {
                    auto &value = columns[item_index(current_item(menu))].Hidden;
                    value = !value;
                }
                break;
        }
    }

    unpost_menu(menu);
    free_menu(menu);

    for (auto &item : items)
        if (item)
            free_item(item);

    delwin(sub);
    delwin(win);
    Refresh();
}

///////////////////////////////////////////////////////

void TPortoValueCache::Register(const std::string &container,
                                const std::string &variable) {
    if (Containers.find(container) == Containers.end())
        Containers[container] = 1;
    else
        Containers[container]++;
    if (Variables.find(variable) == Variables.end())
        Variables[variable] = 1;
    else
        Variables[variable]++;
}
void TPortoValueCache::Unregister(const std::string &container,
                                  const std::string &variable) {
    auto c = Containers.find(container);
    if (c != Containers.end()) {
        if (c->second == 1)
            Containers.erase(c);
        else
            c->second--;
    }
    auto v = Variables.find(variable);
    if (v != Variables.end()) {
        if (v->second == 1)
            Variables.erase(v);
        else
            v->second--;
    }
}

std::string TPortoValueCache::GetValue(const std::string &container,
                                       const std::string &variable,
                                       bool prev) {
    return Cache[CacheSelector ^ prev][container][variable];
}

uint64_t TPortoValueCache::GetDt() {
    return Time[CacheSelector] - Time[!CacheSelector];
}

int TPortoValueCache::Update(Porto::TPortoApi &api) {
    std::vector<std::string> _containers;
    for (auto &iter : Containers)
        _containers.push_back(iter.first);

    std::vector<std::string> _variables;
    for (auto &iter : Variables)
        _variables.push_back(iter.first);;

    CacheSelector = !CacheSelector;
    Cache[CacheSelector].clear();
    auto rsp = api.Get(_containers, _variables, Porto::GET_SYNC | Porto::GET_REAL);
    if (rsp) {
        for (auto &ct: rsp->list()) {
            auto &ct_cache = Cache[CacheSelector][ct.name()];
            for (auto &kv: ct.keyval())
                ct_cache[kv.variable()] = kv.value();
        }
    }
    Time[CacheSelector] = GetCurrentTimeMs();

    return api.GetVersion(Version, Revision);
}

TPortoValue::TPortoValue() : Flags(ValueFlags::Raw), Cache(nullptr), Container(nullptr) {
}

TPortoValue::TPortoValue(const TPortoValue &src) :  Flags(src.Flags),
    Cache(src.Cache), Container(src.Container), Variable(src.Variable),
    Multiplier(src.Multiplier) {
    if (Cache && Container)
        Cache->Register(Container->GetName(), Variable);
}

TPortoValue::TPortoValue(const TPortoValue &src, std::shared_ptr<TPortoContainer> &container) :
    Flags(src.Flags), Cache(src.Cache), Container(container), Variable(src.Variable),
    Multiplier(src.Multiplier) {
    if (Cache && Container)
        Cache->Register(Container->GetName(), Variable);
}

TPortoValue::TPortoValue(std::shared_ptr<TPortoValueCache> &cache,
                         std::shared_ptr <TPortoContainer> &container,
                         const std::string &variable, int flags, double multiplier) :
    Flags(flags), Cache(cache), Container(container), Variable(variable),
    Multiplier(multiplier) {
    if (Cache && Container)
        Cache->Register(Container->GetName(), Variable);
}

TPortoValue::~TPortoValue() {
    if (Cache && Container)
        Cache->Unregister(Container->GetName(), Variable);

    Container = nullptr;
}

void TPortoValue::Process() {
    if (!Container) {
        AsString = "";
        return;
    }

    if (Flags & ValueFlags::Container) {
        std::string name = Container->GetName();
        std::string tab = "", tag = "";

        int level = Container->GetLevel();

        if (name != "/")
            name = name.substr(1 + name.rfind('/'));

        tab = std::string(level, ' ');

        if (Container->Tag & PortoTreeTags::Self)
            tag = "@ ";

        else if (level)
            tag = Container->ChildrenCount() ? "+ " : "- ";

        AsString = tab + tag + name;
        return;
    }

    AsString = Cache->GetValue(Container->GetName(), Variable, false);

    if (Flags & ValueFlags::State) {
        AsNumber = 0;
        if (AsString == "running")
            AsNumber = 1000;
        else if (AsString == "meta")
            AsNumber = 500;
        else if (AsString == "starting")
            AsNumber = 300;
        else if (AsString == "paused")
            AsNumber = 200;
        else if (AsString == "dead")
            AsNumber = 100;

        AsNumber += Container->ChildrenCount();

        return;
    }

    if (Flags & ValueFlags::NetState) {
        auto sep = AsString.find(' ');
        if (sep != std::string::npos)
            AsString = AsString.substr(0, sep);
        if (AsString == "L3")
            AsNumber = 2;
        else if (AsString != "")
            AsNumber = 1;
        else
            AsNumber = 0;
        return;
    }

    if (Flags & ValueFlags::Chroot) {
        if (AsString == "" || AsString == "/") {
            AsString = "";
            AsNumber = 0;
        } else {
            AsString = "true";
            AsNumber = 1;
        }
        return;
    }

    if ((Flags & ValueFlags::Raw) || AsString.length() == 0) {
        AsNumber = -1;
        return;
    }

    AsNumber = ParseValue(AsString, Flags & ValueFlags::Map);

    if (Flags & ValueFlags::DfDt) {
        std::string old = Cache->GetValue(Container->GetName(), Variable, true);
        if (old.length() == 0)
            old = AsString;
        AsNumber = DfDt(AsNumber, ParseValue(old, Flags & ValueFlags::Map), Cache->GetDt());
    }

    if (Flags & ValueFlags::PartOfRoot) {
        std::string root_raw = Cache->GetValue("/", Variable, false);
        double root_number;

        root_number = ParseValue(root_raw, Flags & ValueFlags::Map);

        if (Flags & ValueFlags::DfDt) {
            std::string old = Cache->GetValue("/", Variable, true);
            if (old.length() == 0)
                old = root_raw;
            root_number = DfDt(root_number, ParseValue(old, Flags & ValueFlags::Map), Cache->GetDt());
        }

        AsNumber = PartOf(AsNumber, root_number);
    }

    if (Flags & ValueFlags::Multiplier)
        AsNumber /= Multiplier;

    if (Flags & ValueFlags::Percents)
        AsString = StringFormat("%.1f", AsNumber * 100);
    else if (Flags & ValueFlags::Seconds)
        AsString = StringFormatDuration(AsNumber * 1000);
    else if (Flags & ValueFlags::Bytes)
        AsString = StringFormatSize(AsNumber);
    else
        AsString = StringFormat("%g", AsNumber);
}
std::string TPortoValue::GetValue() const {
    return AsString;
}
int TPortoValue::GetLength() const {
    return AsString.length();
}
bool TPortoValue::operator< (const TPortoValue &v) {
    if (Flags & ValueFlags::Raw)
        return AsString < v.AsString;
    else if (Flags & ValueFlags::Container)
        return Container->GetName() < v.Container->GetName();
    else
        return AsNumber > v.AsNumber;
}

TCommonValue::TCommonValue(const std::string &label, const TPortoValue &val) :
    Label(label), Value(val) {
}
std::string TCommonValue::GetLabel() {
    return Label;
}
TPortoValue& TCommonValue::GetValue() {
    return Value;
}

TPortoContainer::TPortoContainer(std::string container) : Container(container) {
    if (Container == "/") {
        Level = 0;
    } else {
        auto unprefixed = container.substr(strlen(ROOT_PORTO_NAMESPACE));
        Level = 1 + std::count(unprefixed.begin(), unprefixed.end(), '/');
    }
}
std::shared_ptr<TPortoContainer> TPortoContainer::GetParent(int level) {
    auto parent = Parent.lock();
    if (parent) {
        if (parent->GetLevel() == level)
            return parent;
        else
            return parent->GetParent(level);
    } else
        return nullptr;
}

std::shared_ptr<TPortoContainer> TPortoContainer::ContainerTree(Porto::TPortoApi &api) {
    std::vector<std::string> containers;
    int ret = api.List(containers);
    if (ret)
        return nullptr;

    std::shared_ptr<TPortoContainer> root = nullptr;
    std::shared_ptr<TPortoContainer> curr = nullptr;
    std::shared_ptr<TPortoContainer> prev = nullptr;
    int level = 0;

    std::string self_absolute_name;
    ret = api.GetProperty("self", "absolute_name", self_absolute_name);
    if (ret)
        return nullptr;

    std::string self_porto_ns;
    ret = api.GetProperty("self", "absolute_namespace", self_porto_ns);
    if (ret)
        return nullptr;

    for (auto &ct : containers)
        ct = self_porto_ns + ct;

    if (self_absolute_name != "/") {
        auto parent = self_absolute_name;
        auto pos = parent.size();

        do {
            auto self_parent = parent.substr(0, pos);

            if (self_parent != "/porto" &&
                std::find(containers.begin(), containers.end(), self_parent)
                          == containers.end()) {

                containers.push_back(self_parent);
            }

            pos = pos ? parent.rfind("/", pos - 1) : std::string::npos;
        } while (pos != std::string::npos && pos);
    }

    std::sort(containers.begin(), containers.end());

    root = std::make_shared<TPortoContainer>("/");
    prev = root;
    root->Tag = self_absolute_name == "/" ? PortoTreeTags::Self : PortoTreeTags::None;

    for (auto &c : containers) {
        if (c == "/")
            continue;

        curr = std::make_shared<TPortoContainer>(c);

        if (c == self_absolute_name)
            curr->Tag |= PortoTreeTags::Self;

        level = curr->GetLevel();
        if (level > prev->GetLevel())
            curr->Parent = prev;
        else if (level == prev->GetLevel())
            curr->Parent = prev->Parent;
        else /* level < prev->GetLevel() */
            curr->Parent = prev->GetParent(level - 1);
        curr->Root = root;

        auto parent = curr->Parent.lock();
        if (!parent)
            return nullptr;

        parent->Children.push_back(curr);
        prev = curr;
    }

    return root;
}
std::string TPortoContainer::GetName() {
    return Container;
}
int TPortoContainer::GetLevel() {
    return Level;
}
void TPortoContainer::ForEach(std::function<void (
                              std::shared_ptr<TPortoContainer> &)> fn, int maxlevel) {

    auto self = shared_from_this();

    if (Level <= maxlevel)
        fn(self);
    if (Level < maxlevel)
        for (auto &c : Children)
            c->ForEach(fn, maxlevel);
}
int TPortoContainer::GetMaxLevel() {
    int level = Level;
    for (auto &c : Children)
        if (c->GetMaxLevel() > level)
            level = c->GetMaxLevel();
    return level;
}
std::string TPortoContainer::ContainerAt(int n, int max_level) {
    auto ret = shared_from_this();
    int i = 0;
    ForEach([&] (std::shared_ptr<TPortoContainer> &row) {
            if (i++ == n)
                ret = row;
        }, max_level);
    return ret->GetName();
}
int TPortoContainer::ChildrenCount() {
    return Children.size();
}


TColumn::TColumn(std::string title, std::string desc, TPortoValue var) :

    RootValue(var),
    LeftAligned(var.Flags & ValueFlags::Left),
    Hidden(var.Flags & ValueFlags::Hidden),
    Title(title),
    Description(desc),
    Flags(var.Flags) {

    Width = title.length();
}
int TColumn::PrintTitle(int x, int y, TConsoleScreen &screen) {
    screen.PrintAt(Title, x, y, Width, LeftAligned,
                   A_BOLD |
                   (Selected ? A_STANDOUT : 0) |
                   (Sorted ? A_UNDERLINE : 0));
    return Width;
}
int TColumn::Print(TPortoContainer &row, int x, int y, TConsoleScreen &screen, int attr) {
    std::string p = At(row).GetValue();
    screen.PrintAt(p, x, y, Width, LeftAligned, attr);
    return Width;
}
void TColumn::Update(std::shared_ptr<TPortoContainer> &tree, int maxlevel) {
    tree->ForEach([&] (std::shared_ptr<TPortoContainer> &row) {
            TPortoValue val(RootValue, row);
            Cache.insert(std::make_pair(row->GetName(), val));
        }, maxlevel);
}
TPortoValue& TColumn::At(TPortoContainer &row) {
    return Cache[row.GetName()];
}
void TColumn::Process() {
    for (auto &iter : Cache) {
        iter.second.Process();

        int w = iter.second.GetLength();
        if (w > Width)
            Width = w;
    }
}
int TColumn::GetWidth() {
    return Width;
}
void TColumn::SetWidth(int width) {
    Width = width;
}
void TColumn::ClearCache() {
    Cache.clear();
}
void TPortoContainer::SortTree(TColumn &column, bool invert) {
    Children.sort([&] (std::shared_ptr<TPortoContainer> &row1,
                       std::shared_ptr<TPortoContainer> &row2) {
            return invert ? (column.At(*row2) < column.At(*row1)) :
                            (column.At(*row1) < column.At(*row2));
        });
    for (auto &c : Children)
        c->SortTree(column, invert);
}

void TPortoTop::PrintTitle(int y, TConsoleScreen &screen) {
    int x = FirstX;
    for (auto &c : Columns)
        if (!c.Hidden)
            x += 1 + c.PrintTitle(x, y, screen);
}
int TPortoTop::PrintCommon(TConsoleScreen &screen) {
    int x = 0;
    int y = 0;
    for (auto &line : Common) {
        for (auto &item : line) {
            std::string p = item.GetLabel();
            screen.PrintAt(p, x, y, p.length());
            x += p.length();
            p = item.GetValue().GetValue();
            screen.PrintAt(p, x, y, p.length(), false, A_BOLD);
            x += p.length() + 1;
        }

        if (!y) {
            std::string p = "Version: ";
            screen.PrintAt(p, x, y, p.length());
            x += p.length();
            p = Cache->Version;
            screen.PrintAt(p, x, y, p.length(), false, A_BOLD);
            x += p.length() + 1;

            p = "Update: ";
            screen.PrintAt(p, x, y, p.length());
            x += p.length();
            p = Paused ? "paused" : StringFormatDuration(Delay);
            screen.PrintAt(p, x, y, p.length(), false, A_BOLD);
        }

        y++;
        x = 0;
    }
    return y;
}

void TPortoTop::Update() {
    for (auto &column : Columns)
        column.ClearCache();
    ContainerTree = TPortoContainer::ContainerTree(*Api);
    if (!ContainerTree)
        return;
    for (auto &column : Columns)
        column.Update(ContainerTree, MaxLevel);
    Cache->Update(*Api);
    Process();
}

void TPortoTop::Process() {
    for (auto &column : Columns)
        column.Process();
    for (auto &line : Common)
        for (auto &item : line)
            item.GetValue().Process();
    Sort();
}

void TPortoTop::Sort() {
    if (ContainerTree)
        ContainerTree->SortTree(Columns[SortColumn], Invert);
}

void TPortoTop::Print(TConsoleScreen &screen) {

    screen.Erase();

    if (!ContainerTree)
        return;

    int width = 0;
    for (auto &column : Columns)
        if (!column.Hidden)
            width += column.GetWidth();

    if (Columns[0].GetWidth() > screen.Width() / 2)
        Columns[0].SetWidth(screen.Width() / 2);

    int at_row = 1 + PrintCommon(screen);

    MaxRows = 0;
    ContainerTree->ForEach([&] (std::shared_ptr<TPortoContainer> &row) {
            if (SelectedContainer == "self" && (row->Tag & PortoTreeTags::Self))
                SelectedContainer = row->GetName();
            if (row->GetName() == SelectedContainer)
                SelectedRow = MaxRows;
            MaxRows++;
        }, MaxLevel);
    DisplayRows = std::min(screen.Height() - at_row, MaxRows);

    PrintTitle(at_row - 1, screen);
    int y = 0;
    SelectedContainer = "";
    ContainerTree->ForEach([&] (std::shared_ptr<TPortoContainer> &row) {
            if (y >= FirstRow && y < MaxRows) {
                bool selected = y == SelectedRow;
                if (selected)
                    SelectedContainer = row->GetName();
                int x = FirstX;

                int attr = 0;
                if (selected)
                    attr |= A_REVERSE;
                auto col = RowColor.find(row->GetName());
                if (col != RowColor.end())
                    attr |= COLOR_PAIR(col->second);

                for (auto &c : Columns) {
                    if (!(c.Flags & ValueFlags::Container) && row->GetLevel() == 1)
                        attr |= A_BOLD;
                    if (!c.Hidden)
                        x += 1 + c.Print(*row, x, at_row + y - FirstRow,
                                         screen, attr);
                }
            }
            y++;
        }, MaxLevel);
    screen.Refresh();
}

void TPortoTop::MarkRow() {
    if (RowColor.count(SelectedContainer)) {
        RowColor.erase(SelectedContainer);
    } else {
        RowColor[SelectedContainer] = NextColor;
        if (++NextColor > 6)
            NextColor = 1;
    }
}

void TPortoTop::AddColumn(const TColumn &c) {
    Columns.push_back(c);
}
bool TPortoTop::AddColumn(std::string title, std::string signal,
                          std::string desc, int flags) {
    size_t off = 0;
    std::string data;

    if (signal == "state")
        flags |= ValueFlags::State;

    if (signal.length() > 4 && signal[0] == 'S' && signal[1] == '(') {
        off = signal.find(')');
        data = signal.substr(2, off == std::string::npos ?
                           std::string::npos : off - 2);
        flags |= ValueFlags::Map;
        if (off != std::string::npos)
            off++;
    } else {
        off = signal.find('\'');
        if (off == std::string::npos)
            off = signal.rfind(' ');
        if (off == std::string::npos)
            off = signal.find('%');

        data = signal.substr(0, off);
    }

    double multiplier = 1;

    if (off != std::string::npos) {
        for (; off < signal.length(); off++) {
            switch (signal[off]) {
            case 'b':
            case 'B':
                flags |= ValueFlags::Bytes;
                break;
            case 's':
            case 'S':
                flags |= ValueFlags::Seconds;
                break;
            case '\'':
                flags |= ValueFlags::DfDt;
                break;
            case '/':
                flags |= ValueFlags::PartOfRoot;
                break;
            case '%':
                flags |= ValueFlags::Percents;
                break;
            case ' ':
                break;
            default:
                {
                    char *endp;
                    multiplier = strtod(signal.c_str() + off, &endp);
                    off = endp - signal.c_str();
                    flags |= ValueFlags::Multiplier;
                }
                break;
            }
        }
    }

    TPortoValue v(Cache, RootContainer, data, flags, multiplier);
    Columns.push_back(TColumn(title, desc, v));
    return true;
}

void TPortoTop::ChangeSelection(int x, int y, TConsoleScreen &screen) {
    SelectedRow += y;

    if (SelectedRow < 0)
        SelectedRow = 0;

    if (SelectedRow >= MaxRows)
        SelectedRow = MaxRows - 1;

    if (SelectedRow < FirstRow)
        FirstRow = SelectedRow;

    if (SelectedRow >= FirstRow + DisplayRows)
        FirstRow = SelectedRow - DisplayRows + 1;

    if (FirstRow + DisplayRows > MaxRows)
        FirstRow = MaxRows - DisplayRows;

    Columns[SelectedColumn].Selected = false;

    SelectedColumn += x;
    if (SelectedColumn < 0) {
        SelectedColumn = 0;
    } else if (SelectedColumn > (int)Columns.size() - 1) {
        SelectedColumn = Columns.size() - 1;
    }
    while (Columns[SelectedColumn].Hidden && x < 0 && SelectedColumn > 0)
        SelectedColumn--;
    while (Columns[SelectedColumn].Hidden && SelectedColumn < (int)Columns.size() - 1)
        SelectedColumn++;
    while (Columns[SelectedColumn].Hidden && SelectedColumn > 0)
        SelectedColumn--;

    Columns[SelectedColumn].Selected = true;

    if (y)
        SelectedContainer = "";

    if (x) {
        int i = 0;
        int _x = FirstX;
        for (auto &c : Columns) {
            if (i == SelectedColumn && _x <= 0) {
                FirstX -= _x;
                _x = 0;
            }
            if (!c.Hidden)
                _x += c.GetWidth() + 1;
            if (i == SelectedColumn && _x > screen.Width()) {
                FirstX -= _x - screen.Width();
                _x = screen.Width();
            }
            i++;
        }
        if (FirstX < 0 && _x < screen.Width())
            FirstX += std::min(screen.Width() - _x, -FirstX);
    }
}

void TPortoTop::ChangeView(int x, int y) {
    FirstX += x;
    if (FirstX > 0)
        FirstX = 0;
    FirstRow += y;
}

void TPortoTop::Expand() {
    if (MaxLevel == 1)
        MaxLevel = 2;
    else if (MaxLevel == 2)
        MaxLevel = 100;
    else
        MaxLevel = 1;
    Update();
}
int TPortoTop::StartStop() {
    std::string state;
    int ret = Api->GetProperty(SelectedContainer, "state", state);
    if (ret)
        return ret;
    if (state == "running" || state == "dead" || state == "meta")
        return Api->Stop(SelectedContainer);
    else
        return Api->Start(SelectedContainer);
}
int TPortoTop::PauseResume() {
    std::string state;
    int ret = Api->GetProperty(SelectedContainer, "state", state);
    if (ret)
        return ret;
    if (state == "paused")
        return Api->Resume(SelectedContainer);
    else
        return Api->Pause(SelectedContainer);
}
int TPortoTop::Kill(int signal) {
    return Api->Kill(SelectedContainer, signal);
}
int TPortoTop::Destroy() {
    return Api->Destroy(SelectedContainer);
}
void TPortoTop::LessPortoctl(std::string container, std::string cmd) {
    std::string s = "portoctl get " + container + " " + cmd + " | less";
    int status = system(s.c_str());
    (void)status;
}

int TPortoTop::RunCmdInContainer(TConsoleScreen &screen, std::string cmd) {
    bool enter = (SelectedContainer != "/" && SelectedContainer != "self");
    int ret = -1;

    screen.Save();
    switch (fork()) {
    case -1:
        ret = errno;
        break;
    case 0:
    {
        if (enter)
            exit(execlp("portoctl", "portoctl",
                        "shell", SelectedContainer.c_str(), cmd.c_str(), nullptr));
        else
            exit(execlp(cmd.c_str(), cmd.c_str(), nullptr));
        break;
    }
    default:
    {
        wait(&ret);
        break;
    }
    }
    screen.Restore();

    if (ret)
        screen.Dialog(strerror(ret), {"Ok"});

    return ret;
}
void TPortoTop::AddCommon(int row, const std::string &title, const std::string &var,
                          std::shared_ptr<TPortoContainer> &container,
                          int flags, double multiplier) {
    Common.resize(row + 1);
    TPortoValue v(Cache, container, var, flags, multiplier);
    Common[row].push_back(TCommonValue(title, v));
}
TPortoTop::TPortoTop(Porto::TPortoApi *api, const std::vector<std::string> &args) :
    Api(api),
    Cache(std::make_shared<TPortoValueCache>()),
    RootContainer(std::make_shared<TPortoContainer>("/")) {

    (void)args;

    AddCommon(0, "Containers running: ", "porto_stat[running]", RootContainer, ValueFlags::Raw);
    AddCommon(0, "of ", "porto_stat[containers]", RootContainer, ValueFlags::Raw);
    AddCommon(0, "Volumes: ", "porto_stat[volumes]", RootContainer, ValueFlags::Raw);
    AddCommon(0, "Networks: ", "porto_stat[networks]", RootContainer, ValueFlags::Raw);
    AddCommon(0, "Clients: ", "porto_stat[clients]", RootContainer, ValueFlags::Raw);
    AddCommon(0, "Uptime: ", "porto_stat[porto_uptime]", RootContainer, ValueFlags::Seconds);

    AddCommon(1, "Started: ", "porto_stat[containers_started]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "Failed: ", "porto_stat[containers_failed_start]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "Errors: ", "porto_stat[errors]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "Warnings: ", "porto_stat[warnings]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "Unknown: ", "porto_stat[fail_system]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "OOM: ", "porto_stat[containers_oom]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "CPS: ", "porto_stat[clients_connected]", RootContainer, ValueFlags::DfDt);
    AddCommon(1, "RPS: ", "porto_stat[requests_completed]", RootContainer, ValueFlags::DfDt);
    AddCommon(1, "FPS: ", "porto_stat[requests_failed]", RootContainer, ValueFlags::DfDt);
    AddCommon(1, "VAL: ", "porto_stat[fail_invalid_value]", RootContainer, ValueFlags::Raw);
    AddCommon(1, "CMD: ", "porto_stat[fail_invalid_command]", RootContainer, ValueFlags::Raw);

    AddColumn(TColumn("Container", "Container name",
              TPortoValue(Cache, ContainerTree, "absolute_name", ValueFlags::Container | ValueFlags::Left | ValueFlags::Always)));

    AddColumn("State", "state", "Current state", ValueFlags::State | ValueFlags::Porto | ValueFlags::Always);
    AddColumn("Time", "time s", "Time elapsed since start or death", ValueFlags::Porto);

    /* CPU */
    AddColumn("Cpu%", "cpu_usage'% 1e9", "Cpu usage in core%", ValueFlags::Cpu);
    AddColumn("Sys%", "cpu_usage_system'% 1e9", "System cpu usage in core%", ValueFlags::Cpu);
    AddColumn("Wait%", "cpu_wait'% 1e9", "Cpu wait time in core%", ValueFlags::Cpu);
    AddColumn("IO-W%", "io_time[hw]'% 1e9", "Cpu waiting for disk IO", ValueFlags::Cpu);
    AddColumn("Thld%", "cpu_throttled'% 1e9", "Cpu throttled time in core%", ValueFlags::Cpu);

    AddColumn("C pol", "cpu_policy", "Cpu scheduler policy", ValueFlags::Raw | ValueFlags::Cpu | ValueFlags::Porto);
    AddColumn("C g-e", "cpu_guarantee", "Cpu guarantee in cores", ValueFlags::Cpu);
    AddColumn("C lim", "cpu_limit", "Cpu limit in cores", ValueFlags::Cpu);

    AddColumn("Ct lim", "cpu_limit_total", "Cpu total limit in cores", ValueFlags::Cpu);
    AddColumn("Ct g-e", "cpu_guarantee_total", "Cpu total guarantee in cores", ValueFlags::Cpu);

    AddColumn("Threads", "thread_count", "Threads count", ValueFlags::Cpu);
    AddColumn("Th lim", "thread_limit", "Threads limit", ValueFlags::Cpu);

    /* Memory */
    AddColumn("Memory", "memory_usage b", "Memory usage", ValueFlags::Mem);
    AddColumn("M g-e", "memory_guarantee b", "Memory guarantee", ValueFlags::Mem);
    AddColumn("M lim", "memory_limit b", "Memory limit", ValueFlags::Mem);
    AddColumn("Free/s", "memory_reclaimed' b", "Memory freed", ValueFlags::Mem);

    AddColumn("Anon", "anon_usage b", "Anonymous memory usage", ValueFlags::Mem);
    AddColumn("Alim", "anon_limit b", "Anonymous memory limit", ValueFlags::Mem);

    AddColumn("Cache", "cache_usage b", "Cache memory usage", ValueFlags::Mem);

    AddColumn("Htlb", "hugetlb_usage b", "HugeTLB memory usage", ValueFlags::Mem);
    AddColumn("Hlim", "hugetlb_limit b", "HugeTLB memory limit", ValueFlags::Mem);

    AddColumn("Mt lim", "memory_limit_total b", "Memory total limit", ValueFlags::Mem);
    AddColumn("Mt g-e", "memory_guarantee_total b", "Memory total guarantee", ValueFlags::Mem);

    AddColumn("OOM", "porto_stat[container_oom]", "Count of OOM events", ValueFlags::Mem);
    AddColumn("OOM-K", "", "Count of OOM kills", ValueFlags::Mem);
    AddColumn("OOM-F", "oom_is_fatal", "OOM is fatal", ValueFlags::Raw | ValueFlags::Mem | ValueFlags::Porto);

    /* I/O */
    AddColumn("Maj/s", "major_faults'", "Major page fault count", ValueFlags::Mem | ValueFlags::Io);
    AddColumn("Min/s", "minor_faults'", "Minor page fault count", ValueFlags::Mem | ValueFlags::Io);

    AddColumn("IO pol", "io_policy", "IO policy", ValueFlags::Raw | ValueFlags::Io | ValueFlags::Porto);

    AddColumn("IO load", "io_time[hw]' 1e9", "Average disk queue depth", ValueFlags::Io);

    AddColumn("IO op/s", "io_ops[hw]'", "IO operations per second", ValueFlags::Io);
    AddColumn("IO read b/s", "io_read[hw]' b", "IO bytes read from disk", ValueFlags::Io);
    AddColumn("IO write b/s", "io_write[hw]' b", "IO bytes written to disk", ValueFlags::Io);

    AddColumn("FS op/s", "io_ops[fs]'", "IO operations by fs", ValueFlags::Io);
    AddColumn("FS read b/s", "io_read[fs]' b", "IO bytes read by fs", ValueFlags::Io);
    AddColumn("FS write b/s", "io_write[fs]' b", "IO bytes written by fs", ValueFlags::Io);

    /* Network */
    AddColumn("Net", "net", "Network config", ValueFlags::NetState | ValueFlags::Net | ValueFlags::Porto);

    AddColumn("Net TC", "net_bytes[Uplink]' b", "Uplink bytes transmitted at tc", ValueFlags::Net);
    AddColumn("Net TX", "net_tx_bytes[Uplink]' b", "Uplink bytes transmitted", ValueFlags::Net);
    AddColumn("Net RX", "net_rx_bytes[Uplink]' b", "Uplink bytes received", ValueFlags::Net);

    AddColumn("Pkt TC", "net_packets[Uplink]'", "Uplink packets transmitted at tc", ValueFlags::Net);
    AddColumn("Pkt TX", "net_tx_packets[Uplink]'", "Uplink packets transmitted", ValueFlags::Net);
    AddColumn("Pkt RX", "net_rx_packets[Uplink]'", "Uplink packets received", ValueFlags::Net);

    AddColumn("Drp TC", "net_drops[Uplink]'", "Uplink TC dropped packets", ValueFlags::Net);
    AddColumn("Drp TX", "net_tx_drops[Uplink]'", "Uplink TX dropped packets", ValueFlags::Net);
    AddColumn("Drp RX", "net_rx_drops[Uplink]'", "Uplink RX dropped packets", ValueFlags::Net);

    AddColumn("TX g-e", "net_guarantee[default] b", "Default network TX guarantee", ValueFlags::Net);
    AddColumn("TX lim", "net_limit[default] b", "Default network TX limit", ValueFlags::Net);
    AddColumn("RX lim", "net_rx_limit[default] b", "Default network RX limit", ValueFlags::Net);

    AddColumn("ToS", "net_tos", "Default traffic class selector", ValueFlags::Raw | ValueFlags::Net | ValueFlags::Porto);

    for (int i = 0; i<8; i++) {
        std::string cs = std::to_string(i);
        AddColumn("CS" + cs, "net_bytes[CS" + cs + "]' b", "Uplink bytes CS" + cs, ValueFlags::Net);
        AddColumn("Pk" + cs, "net_packets[CS" + cs + "]'", "Uplink packets CS" + cs, ValueFlags::Net);
        AddColumn("Dp" + cs, "net_drops[CS" + cs + "]'", "Uplink dropped CS" + cs, ValueFlags::Net);
    }

    /* Porto */
    AddColumn("ID", "id", "Container id", ValueFlags::Raw | ValueFlags::Porto);
    AddColumn("L", "level", "Container level", ValueFlags::Raw | ValueFlags::Porto);

    AddColumn("Isolate", "isolate", "Container with pid-namespace", ValueFlags::Raw | ValueFlags::Porto);
    AddColumn("VMode", "virt_mode", "Porto virt mode", ValueFlags::Raw | ValueFlags::Porto);
    AddColumn("Chroot", "root", "Container with chroot", ValueFlags::Chroot | ValueFlags::Porto);

    AddColumn("Porto", "enable_porto", "Porto access level", ValueFlags::Raw | ValueFlags::Porto);
    AddColumn("Cli", "porto_stat[container_clients]", "Porto clients", ValueFlags::Porto);
    AddColumn("RPS", "porto_stat[container_requests]'", "Porto requests/s", ValueFlags::Porto);

    AddColumn("Core", "CORE.dumped", "Cores dumped", ValueFlags::Porto);
    AddColumn("Respawn", "respawn_count", "Respawn count", ValueFlags::Porto);
}

static bool exit_immediatly = false;
void exit_handler(int) {
    exit_immediatly = true;
}

int portotop(Porto::TPortoApi *api, const std::vector<std::string> &args) {
    Signal(SIGINT, exit_handler);
    Signal(SIGTERM, exit_handler);
    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGTTIN, SIG_IGN);

    TPortoTop top(api, args);

    top.SelectedContainer = "self";
    top.Columns[top.SelectedColumn].Selected = true;
    top.Columns[top.SortColumn].Sorted = true;

    top.Update();

    /* Main loop */
    TConsoleScreen screen;

    bool first = true;

    screen.SetTimeout(top.FirstDelay);

    while (true) {
        if (exit_immediatly)
            break;

        top.Print(screen);

        int button = screen.Getch();
        switch (button) {
        case ERR:
            if (!top.Paused)
                top.Update();
            break;
        case 'q':
        case 'Q':
            return EXIT_SUCCESS;
            break;
        case KEY_UP:
            top.ChangeSelection(0, -1, screen);
            break;
        case KEY_PPAGE:
            top.ChangeSelection(0, -10, screen);
            break;
        case KEY_DOWN:
            top.ChangeSelection(0, 1, screen);
            break;
        case KEY_NPAGE:
            top.ChangeSelection(0, 10, screen);
            break;
        case KEY_LEFT:
            top.ChangeSelection(-1, 0, screen);
            break;
        case KEY_RIGHT:
            top.ChangeSelection(1, 0, screen);
            break;
        case KEY_HOME:
            top.ChangeSelection(-1000, 0, screen);
            break;
        case KEY_END:
            top.ChangeSelection(1000, 0, screen);
            break;
        case '<':
            top.ChangeView(1, 0);
            break;
        case '>':
            top.ChangeView(-1, 0);
            break;
        case '\t':
            top.Expand();
            break;
        case ' ':
            top.Paused = !top.Paused;
            break;
        case 'f':
            screen.ColumnsMenu(top.Columns);
            break;
        case KEY_DC:
        case 'd':
            if (top.SelectedColumn > 0) {
                top.Columns[top.SelectedColumn].Hidden ^= true;
                top.ChangeSelection(1, 0, screen);
            }
            break;
        case 'a':
            for (auto &c: top.Columns)
                c.Hidden = false;
            break;
        case 'c':
            for (auto &c: top.Columns)
                c.Hidden = !(c.Flags & (ValueFlags::Always | ValueFlags::Cpu));
            break;
        case 'm':
            for (auto &c: top.Columns)
                c.Hidden = !(c.Flags & (ValueFlags::Always | ValueFlags::Mem));
            break;
        case 'i':
            for (auto &c: top.Columns)
                c.Hidden = !(c.Flags & (ValueFlags::Always | ValueFlags::Io));
            break;
        case 'n':
            for (auto &c: top.Columns)
                c.Hidden = !(c.Flags & (ValueFlags::Always | ValueFlags::Net));
            break;
        case 'p':
            for (auto &c: top.Columns)
                c.Hidden = !(c.Flags & (ValueFlags::Always | ValueFlags::Porto));
            break;
        case KEY_BACKSPACE:
            if (top.SelectedColumn > 1) {
                top.SelectedColumn--;
                std::swap(top.Columns[top.SelectedColumn],
                          top.Columns[top.SelectedColumn + 1]);
            }
            break;
        case 's':
        case '\n':
            if (top.SortColumn != top.SelectedColumn) {
                top.Columns[top.SortColumn].Sorted = false;
                top.SortColumn = top.SelectedColumn;
                top.Columns[top.SortColumn].Sorted = true;
                top.Invert = false;
            } else {
                top.Invert = !top.Invert;
            }
            top.Sort();
            break;
        case 'S':
            if (screen.Dialog("Start/stop container " + top.SelectedContainer,
                              {"No", "Yes"}) == 1) {
                if (top.StartStop())
                    screen.ErrorDialog(*api);
                else
                    top.Update();
            }
            break;
        case 'P':
            if (screen.Dialog("Pause/resume container " + top.SelectedContainer,
                              {"No", "Yes"}) == 1) {
                if (top.PauseResume())
                    screen.ErrorDialog(*api);
                else
                    top.Update();
            }
            break;
        case 'K':
        {
            int signal = -1;
            switch (screen.Dialog("Kill container " + top.SelectedContainer,
                                  {"Cancel", "SIGTERM", "SIGINT", "SIGKILL", "SIGHUP"})) {
            case 1:
                signal = SIGTERM;
                break;
            case 2:
                signal = SIGINT;
                break;
            case 3:
                signal = SIGKILL;
                break;
            case 4:
                signal = SIGHUP;
                break;
            }
            if (signal > 0) {
                if (top.Kill(signal))
                    screen.ErrorDialog(*api);
                else
                    top.Update();
            }
            break;
        }
        case 'D':
            if (screen.Dialog("Destroy container " + top.SelectedContainer,
                              {"No", "Yes"}) == 1) {
                if (top.Destroy())
                    screen.ErrorDialog(*api);
                else
                    top.Update();
            }
            break;
        case 't':
            top.RunCmdInContainer(screen, "top");
            break;
        case 'b':
            top.RunCmdInContainer(screen, "bash");
            break;
        case 'g':
            screen.Save();
            top.LessPortoctl(top.SelectedContainer, "");
            screen.Restore();
            break;
        case 'o':
            screen.Save();
            top.LessPortoctl(top.SelectedContainer, "stdout");
            screen.Restore();
            break;
        case 'e':
            screen.Save();
            top.LessPortoctl(top.SelectedContainer, "stderr");
            screen.Restore();
            break;
        case '0':
            top.Delay = 10000;
            top.Paused = false;
            screen.SetTimeout(top.Delay);
            break;
        case '1'...'9':
            top.Delay = (button - '0') * 1000;
            top.Paused = false;
            screen.SetTimeout(top.Delay);
            break;
        case 'u':
            top.Update();
            screen.Clear();
            break;
        case '!':
            top.MarkRow();
            break;
        case '@':
            top.SelectedContainer = "self";
            break;
        case 0:
        case KEY_RESIZE:
        case KEY_MOUSE:
            break;
        case 'h':
        case '?':
        default:
            screen.HelpDialog();
            break;
        }

        if (first) {
            first = false;
            screen.SetTimeout(top.Delay);
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    Porto::TPortoApi api;

    return portotop(&api, args);
}
