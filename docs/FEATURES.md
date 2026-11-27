# CargoHatch feature list

CargoHatch is a native Linux file manager for power users. This is the project's single list of features: what's built, what's planned, and ideas for later. It doubles as a map of where the project could go, so most rows are still `Idea`.

**Linux fit**, used in the right-hand column:
- **L**: fits Linux as it is
- **A**: needs a Linux adaptation (the adaptation is noted)
- **W**: only makes sense on Windows, so skip or replace

---

## How to use this list

- Pick features by ID (e.g. `NAV-03`, `COPY-12`). Each ID is one implementable, testable unit, and IDs never change. Items split from a bigger one take the next free number in their section.
- New ideas go in the **Ideas inbox** as soon as they come up, then move into a section once discussed.
- A feature's row is updated in the same change that builds it.
- [Suggested order](#suggested-order-a-design-choice-the-user-decides) and [Done by release](#done-by-release) are near the end.

**Status:** `Idea` (captured) · `Planned` (wanted) · `Doing` (being built) · `Done vX.Y.Z` (shipped; `Done (unreleased, <commit>)` until the release) · `Dropped` (reason in Notes)

**Size:** `S` an afternoon · `M` a day or two · `L` several changes

---

## Ideas inbox

| ID | Idea | Notes |
|---|---|---|
| IDEA-01 | An editor for folder tab-colour rules | Rules live in `cargohatch.conf` until there's a preferences window (CFG-01) |
| IDEA-02 | Keep the recently closed windows list across restarts | Today only the last closed window survives a restart |
| IDEA-03 | Save window state when the session ends (SIGTERM, logout), not only on close | Found while doing LST-20 |
| IDEA-04 | Open the desktop's folder chooser on top of the CargoHatch window (pass the window handle to the portal) | Copy To… "Browse" opens it unparented for now |
| IDEA-05 | Start renaming by clicking the name of an already selected item (slow double-click) | Other file managers offer it as an option; REN-01 has F2 only |
| IDEA-06 | Configurable rename control keys, each selecting a part of the name or applying a preset | A common option for heavy renamers |
| IDEA-09 | Check cut, copy and paste against Files, Dolphin and Thunar on Wayland | Formats are tested, other programs are not |
| IDEA-13 | Rename, delete and create folders from the folder tree | Those keys do nothing there today, so they can't act on the file list by mistake |
| IDEA-14 | Drag folders onto the favourites bar, and drag its items to reorder them | Came up with FAV-01; drag and drop is COPY-20 |
| IDEA-19 | Open several files of one type in one application window (all the pictures in one viewer), where the application takes several files | Came up with FCMD-21, which opens each file on its own |

---

## 1. Lister window and layout (LST)

A "Lister" is a file-manager window. Each Lister has one or two file displays, and each display has tabs.

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| LST-01 | Multiple independent Lister windows | L | M | Done v0.1.0 |  |
| LST-02 | Single or **dual file display** (side-by-side or stacked); toggle, flip orientation, remember setting | L | M | Done v0.1.0 |  |
| LST-03 | **Source/destination** concept: one display is source, the other destination; "lock" states (Source, Dest, LockOff) | L | M | Done v0.1.0 |  |
| LST-04 | Option: single-display Listers also have source/dest modes (copy between windows) | L | M | Done v0.1.0 |  |
| LST-05 | When switching to dual: set new panel as source; open a specific folder or tab group in it | L | M | Idea |  |
| LST-06 | Swap the two sides; copy path to the other side; open the item in the other side (`OPENINDUAL/LEFT/RIGHT/DEST`) | L | M | Done v0.1.0 |  |
| LST-07 | Adjustable split size between displays (`DUALSIZE`) | L | S | Done v0.1.0 |  |
| LST-08 | **Folder tree** panel: on/off/toggle; left/right; one tree per side, or one shared tree | L | L | Done v0.2.0 | F9. Content, looks and following are TREE-01/03/04 |
| LST-09 | **Viewer pane** (preview panel): horizontal/vertical, size, lock | L | L | Done (unreleased) | One pane per window, showing the active side's item under the cursor. F11 or View → Viewer Pane; beside or below the files; resized with its splitter; locked from its padlock or the View menu. Remembered with the window and in layouts |
| LST-10 | **Metadata pane** (edit tags/properties of the selection) | L | L | Idea |  |
| LST-11 | **Utility panel** with modes: Find, Simple Find, Advanced Find, Sync, Duplicates, Undo log, File log, FTP log, Script log, Diagnostic log, Email | L | L | Idea |  |
| LST-12 | Status bar (on/off); styles: single, dual, independent, at bottom | L | S | Idea |  |
| LST-13 | **Jobs bar** showing running and queued operations | L | L | Idea |  |
| LST-14 | Lister commands: minimize, maximize, restore, tile H/V, cascade, minimize all, to front | A (via the WM) | S | Idea |  |
| LST-15 | Set Lister position, size and title from a command | A (Wayland limits positioning) | M | Idea |  |
| LST-16 | Window title options: full path, layout name, custom title (with codes); window icon = app or current folder icon | L | S | Idea |  |
| LST-17 | Move top-row toolbars into the window title bar (CSD) | A | M | Idea |  |
| LST-18 | **Styles**: named Lister arrangements (which panels are on, format lock, toolbar), switchable from a menu | L | L | Idea |  |
| LST-19 | **Layouts**: save and restore a set of Listers (paths, tabs, panels, positions); folders of layouts; hide from lists; desktop shortcut to a layout | L | L | Done v0.1.0 | Layout folders and hiding layouts split out as LST-31 |
| LST-20 | **Default Lister**: template for new windows; update automatically on close; positioning rules (fixed, same position, under mouse); virtual-desktop awareness | A | M | Done v0.1.0 | In part: remember on close. Position rules split out as LST-32 |
| LST-21 | Undo close Lister; "Last closed Listers" list | L | S | Done v0.1.0 |  |
| LST-22 | Admin mode: temporarily elevate a Lister with a timeout | A (polkit/pkexec helper) | L | Idea |  |
| LST-23 | Read-only mode for a display | L | S | Idea |  |
| LST-24 | **Navigation Lock**: both sides navigate in step, mirroring relative paths; warns when out of sync | L | L | Idea |  |
| LST-25 | **Paired folders**: folder A automatically pairs with folder B (regex match/replace); auto-dual, auto-NavLock, default sync target | L | L | Idea |  |
| LST-26 | Tab key cycles UI elements or swaps source/dest; Ctrl+Tab opens a tab switcher | L | M | Done v0.1.0 | Ctrl+Tab switcher only. Tab key moving focus split out as LST-33 |
| LST-27 | Focus command: move focus to left/right, tree, path field, file display, panes | L | S | Idea |  |
| LST-28 | Transition animations (navigate into/up/back/forward) | L | M | Idea |  |
| LST-29 | Dark mode: follow system / always dark / always light | L | S | Idea |  |
| LST-30 | UI spacing schemes (compact/normal/touch), switch automatically | L | M | Idea |  |
| LST-31 | Layout folders; hide layouts from lists | L | S | Idea | Split from LST-19 |
| LST-32 | Default window position rules (fixed, same place, under the mouse) | A (Wayland limits positioning) | M | Idea | Split from LST-20 |
| LST-33 | Tab key moves focus between UI elements or swaps source and destination | L | S | Idea | Split from LST-26; worth it once there's a tree or more panes |
| LST-34 | Errors in the status bar clear on the next key or click, or by themselves after 8 seconds | L | S | Done v0.2.0 | Was IDEA-08. Being sent home from a folder that did not open stays until a key or click |
| LST-35 | A single window locked as the destination copies and moves from its paired source window, and says so | L | S | Done v0.2.0 | Was IDEA-17. Its menu says "Copy from Source Window" |

## 2. Folder tabs (TAB)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| TAB-01 | Tabs per file display; new, close, duplicate, move to other side, reorder | L | M | Done v0.1.0 |  |
| TAB-02 | Tab bar position: above, below, left, right (vertical tabs) | L | M | Done v0.1.0 |  |
| TAB-03 | Tab sizing: same-size, fixed size, min/max, shrink to fit, vertical width | L | M | Planned |  |
| TAB-04 | New-tab button; double-click empty bar for a new tab; open new tabs next to the active one | L | S | Done v0.1.0 |  |
| TAB-05 | Overflow menu when tabs exceed the space | L | S | Done v0.1.0 |  |
| TAB-06 | Tab label options: drive letter, folder icon, tab index, truncate with "…" | L | S | Idea |  |
| TAB-07 | Close behaviours: middle-click, double-click, close buttons (small), close on device eject, Lister closes with its last tab | L | M | Done v0.1.0 | Closing tabs on device eject split out as TAB-20 |
| TAB-08 | **Tab locking**: lock, lock with changes allowed, lock-and-reuse; padlock icon | L | M | Done v0.1.0 |  |
| TAB-09 | **Linked tabs** across sides (show the pair together) | L | M | Done v0.1.0 |  |
| TAB-10 | Tab colour and edge accent per folder or wildcard | L | M | Done v0.1.0 |  |
| TAB-11 | Rename tab (custom label) | L | S | Done v0.1.0 |  |
| TAB-12 | Reopen closed tab (undo close) | L | S | Done v0.1.0 |  |
| TAB-13 | **Tab groups**: save and load named sets of tabs (per side, with formats, active tab, locks, links, colours) | L | M | Done v0.1.0 |  |
| TAB-14 | Split tabs to the right into dual display or a new Lister | L | M | Idea |  |
| TAB-15 | Find an existing tab for a folder instead of opening a duplicate | L | S | Done v0.1.0 |  |
| TAB-16 | Per-tab format lock; process file changes in background tabs | L | M | Idea |  |
| TAB-17 | Drag over a tab to activate it after a delay | L | S | Idea |  |
| TAB-18 | Tab home folder (first folder, or a specific folder); `Go HOMEDIR` | L | S | Done v0.1.0 |  |
| TAB-19 | Tab list menu, tab switcher popup | L | M | Done v0.1.0 |  |
| TAB-20 | Close tabs whose device is ejected | A (udisks) | S | Idea | Split from TAB-07 |

## 3. Navigation and the path field (NAV)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| NAV-01 | Back/forward/up/root; history drop-down lists (names only, icons, sorted) | L | M | Done v0.1.0 |  |
| NAV-02 | **Breadcrumb Location Bar** with clickable components, drop-down sub-folder menus, ghost components after going up | L | M | Done v0.1.0 |  |
| NAV-03 | Editable path field with **path completion** (popup list, network server names, ↑/↓ only) | L | M | Done v0.2.0 | Network server names split out as NAV-17 |
| NAV-04 | What the path-field menus include: archives, hidden folders, system folders, favourites, empty drives | L | M | Idea |  |
| NAV-05 | Location Bar buttons: back/forward/up, copy-to-other-side, swap, layout toggle, close | L | M | Idea |  |
| NAV-06 | Drive buttons/menu, filtered by type (fixed, network, removable, optical, RAM disk, MTP) | A (mounts/udisks) | M | Idea |  |
| NAV-07 | **Folder aliases** (e.g. `/downloads`), built-in plus user-defined | L | M | Done v0.2.0 | Written `~downloads`, as in zsh, since `/name` is a real path on Linux; a typed `/name` still works if no such folder exists |
| NAV-08 | Select previous folder when going up; highlight it with a frame | L | S | Done v0.2.0 | By any way up (Up, Back, the location bar, root). The cursor is framed in the accent colour, not selected, so the next key can't act on it by surprise |
| NAV-09 | `..` parent item in listings (optional; hidden when grouped) | L | S | Idea |  |
| NAV-10 | Show folder read errors inline, with "attempt access"/"retry" links | L | M | Idea |  |
| NAV-11 | **Quick Go**: type to jump to folders from history, favourites, aliases, recent, smart favourites | L | M | Idea |  |
| NAV-12 | Limit path / root lock (`LIMITPATH`): the tab can't leave a subtree | L | S | Idea |  |
| NAV-13 | Go to containing folder of a selected item, shortcut or search result | L | S | Idea |  |
| NAV-14 | Mouse: wheel+Shift for back/forward, side buttons | L | S | Idea |  |
| NAV-15 | Eject device from Go | A (udisks) | S | Idea |  |
| NAV-16 | Open a folder in the app from other programs (CLI, desktop file, "Open in…" handler); replace the default file manager for folders | A (xdg-mime inode/directory) | M | Idea |  |
| NAV-17 | Complete network server names in the path field | L | S | Idea | Split from NAV-03; needs NET-01 |
| NAV-18 | Typing or passing a **file's path** opens its folder with the file selected | L | S | Done v0.2.0 | Was IDEA-12. In the location bar, on the command line, and for locked tabs. A hidden file says why it can't be seen |

## 4. File display: view modes and columns (VIEW)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| VIEW-01 | View modes: Large icons, Small icons, List, Details, **Power** (details plus extra mouse/keyboard modes), Thumbnails, Tiles; cycle | L | L | Done v0.2.0 | Thumbnails fixed at 128px, in memory only (see VIEW-08, VIEW-10) |
| VIEW-02 | **Details columns**: a very large set (see the column ideas at the end). Add, remove, reorder, resize, auto-size, freeze columns | L | L | Done v0.2.0 | 12 columns. Frozen columns split out as VIEW-25 |
| VIEW-03 | Per-column appearance: colour, font style, alignment, default width, middle ellipsis, custom title, reverse initial sort | L | M | Idea |  |
| VIEW-04 | Relative **graphs** behind size and date columns; free-space graphs for drives | L | M | Idea |  |
| VIEW-05 | Friendly dates (Today, Monday…); show seconds or milliseconds | L | S | Idea |  |
| VIEW-06 | Grid lines, horizontal or vertical (solid, alternate, dot, dash, fill, colour, opacity) | L | S | Idea |  |
| VIEW-07 | Full-row select; checkbox mode (select by ticking) | L | S | Idea |  |
| VIEW-08 | Thumbnail size (Ctrl+wheel), labels, borders, aspect/relative-dimension overlays, film sprockets on video, rating stars, file-type badge | L | L | Idea |  |
| VIEW-09 | Thumbnail resize modes: fit/reduce, smooth/pixelated, fill-crop | L | S | Idea |  |
| VIEW-10 | **Thumbnail cache** with size cap, per drive type, lossless option, clear; thumbnail threads; preload before visible | L (freedesktop thumbnail spec) | L | Idea |  |
| VIEW-11 | **Folder thumbnails** from `folder.jpg` or from contents (most recent images, look into sub-folders, wildcard); render styles | L | M | Idea |  |
| VIEW-12 | Tiles mode with a configurable field definition (which metadata lines appear) | L | M | Idea | Fixed tiles (name, type, size) came with VIEW-01; this is the configurable part |
| VIEW-13 | **Expandable folders** in the file display: tree-style inline expansion, Alt-click for deeper levels, collapse behaviours | L | L | Idea |  |
| VIEW-14 | **Flat View**: show all files in all sub-folders as one list (modes: mixed, no folders, grouped) | L | L | Idea |  |
| VIEW-15 | **Grouping** by any column, or by evaluator group schemes; collapse groups, order, folders on top, combine single groups | L | L | Idea |  |
| VIEW-16 | Font scale per display mode (Ctrl+wheel) | L | S | Idea |  |
| VIEW-17 | Icon overlays and status icons (cloud sync state, VCS/Tortoise icons) | A (git status) | M | Idea |  |
| VIEW-18 | Name display: hide extensions (all, or by type), show localized names, change ALL-CAPS display, strip LTR/RTL marks | L | M | Idea |  |
| VIEW-19 | Blur filenames (privacy screenshot mode) | L | S | Idea |  |
| VIEW-20 | Info tips (hover tooltips with metadata), configurable per file type | L | M | Idea |  |
| VIEW-21 | Background images per pane, per folder (tile/stretch/fit/fill, opacity) | L | M | Idea |  |
| VIEW-22 | Highlight cells: Ctrl-drag over cells and copy them | L | M | Idea |  |
| VIEW-23 | Copy column data to clipboard (CSV/tab, with headings) | L | S | Idea |  |
| VIEW-24 | Inline editing of the Rating column by clicking | L | S | Idea |  |
| VIEW-25 | Frozen columns: Name stays put while scrolling sideways | L | M | Planned | Split from VIEW-02; pairs well with VIEW-03 |

## 5. Sorting (SORT)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| SORT-01 | Sort by any column, reverse, smart toggle | L | M | Done v0.2.0 |  |
| SORT-02 | Folders first / files first / mixed; keep folders alphabetical | L | M | Done v0.2.0 |  |
| SORT-03 | **Natural numeric** sort; **word sort** (special hyphen handling); sort name and extension separately | L | M | Done v0.2.0 |  |
| SORT-04 | Ignore prefixes ("The ", "A ") when sorting | L | S | Idea |  |
| SORT-05 | **Manual sort**: drag to reorder, with the order saved per folder | L (sidecar/xattr) | M | Idea |  |
| SORT-06 | Sort newly created or copied files into place, or append them at the end | L | S | Idea |  |
| SORT-07 | Shortcuts to folders sort like folders | L (symlinks) | S | Idea |  |

## 6. Folder formats (FMT)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FMT-01 | A **folder format** bundles view mode, columns, sort, grouping, filters, thumbnail size and fonts | L | L | Idea |  |
| FMT-02 | Save a format for one folder, a subtree, or all folders (default); favourite formats | L | M | Idea |  |
| FMT-03 | **Wildcard/regex path formats** (e.g. `*/Photos*` → thumbnails) | L | M | Idea |  |
| FMT-04 | **Content-type formats**: auto-format when ≥ N% of files are images/music/video, with thresholds | L | M | Idea |  |
| FMT-05 | Automatically remember edited formats (LRU cap, clear) | L | M | Idea |  |
| FMT-06 | Format lock (stop formats changing while navigating) per tab or side | L | S | Idea |  |
| FMT-07 | Share a format to or from the clipboard (text) | L | S | Idea |  |
| FMT-08 | Per-folder toolbars, labels and tab colours (folder-matched settings) | L | M | Idea |  |

## 7. Filtering and hiding (FILT)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FILT-01 | **Quick filter / filter bar**: type to filter in real time; wildcard, regex, any-word, partial, ignore diacritics/extension, **evaluator** expressions; clear on folder change or Esc | L | M | Done v0.2.0 | Evaluator expressions split out as FILT-10 |
| FILT-02 | Show/hide filters per format: file-name and folder-name wildcards/regex, attribute filters | L | M | Idea |  |
| FILT-03 | **Global filters**: hide hidden files, hide system files, global wildcard hides | L (dotfiles) | M | Done v0.2.0 | System files on Linux: names in a folder's `.hidden` file. Hiding dotfiles is on by default |
| FILT-04 | **Permanent filters** that can never be shown (e.g. `desktop.ini`) | L | S | Idea |  |
| FILT-05 | Toggle "show everything" per tab (temporarily disable all filters), with an info banner | L | S | Idea |  |
| FILT-06 | **Filter definitions**: a reusable rule builder (name, size, date, attributes, content, type, labels, nested AND/OR, sub-filters), named and shareable | L | L | Idea |  |
| FILT-07 | Filtered copy and delete (apply a filter to operations; toggle filters on/off) | L | M | Idea |  |
| FILT-08 | Type-filter drop-down menu (filter by file type quickly) | L | S | Idea |  |
| FILT-09 | Label filters (show only items with a label) | L | S | Idea |  |
| FILT-10 | Evaluator expressions in the filter bar | L | S | Idea | Split from FILT-01; needs SCR-05 |

## 8. Selection (SEL)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| SEL-01 | Select all / files / dirs / none / invert / reselect previous | L | S | Done v0.2.0 |  |
| SEL-02 | Select by wildcard pattern or regex (Simple dialog), deselect non-matching | L | M | Done v0.2.0 |  |
| SEL-03 | **Advanced select** using a filter definition; actions select, deselect, hide, hide non-matching | L | L | Idea |  |
| SEL-04 | Select by date (newest/oldest), by size (largest/smallest) | L | S | Idea |  |
| SEL-05 | **Compare-select across sides**: select items in source that are/aren't in destination (with or without extension) | L | M | Idea |  |
| SEL-06 | Select similar (same type/extension/sort-field value) | L | S | Idea |  |
| SEL-07 | Hide selected / hide unselected / unhide | L | S | Idea |  |
| SEL-08 | Save and restore named selections | L | S | Idea |  |
| SEL-09 | Navigate selection: first, last, next, prev (mark, row, sort field) | L | S | Idea |  |
| SEL-10 | Select empty folders / zero-byte folders / duplicates | L | S | Idea |  |
| SEL-11 | **Find-as-you-type (FAYT)**: jump to name; highlight matches, scrollbar indicators, anywhere-in-name, first-char repeat mode, prefer shorter names | L | M | Done v0.2.0 | The field stays open until Esc, Enter or a click; a close timeout was left out, as a Backspace just after it would go up a folder |
| SEL-12 | **Quick Keys** prefixes in the FAYT field: `>` command, `?` search, `/` go to alias, `\` filter, plus script-defined prefixes | L | M | Idea |  |
| SEL-13 | Lasso, range select; Power-mode mouse button actions (select, toggle, drag, context menu) | L | M | Idea |  |
| SEL-14 | Single-click mode (hover to select, underline on hover) | L | S | Idea |  |
| SEL-15 | Auto-select first file in folder; auto-select next file after delete; auto-select newly copied files | L | S | Idea | Selecting the next item after a delete came with DEL-02; first file and newly copied files remain |

## 9. Copy, move and file operations (COPY)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| COPY-01 | Copy/move to destination, to a chosen folder (`TO=ask`), or here (duplicate); create the destination folder | L | M | Done v0.2.0 |  |
| COPY-02 | **When-exists policy**: ask, skip, replace, rename new, rename old, resume, merge, keep newer, recycle old | L | M | Done v0.2.0 |  |
| COPY-03 | **Confirm File Replace** dialog: side-by-side info and thumbnails, Resume/Replace/Skip/Rename with drop-down variants, apply to all, rename presets | L | M | Done v0.2.0 | "Choose a Name…" instead of rename presets; no "make All the default" option |
| COPY-04 | **Update copy**: only when newer, larger or smaller; size/date tolerance | L | M | Idea |  |
| COPY-05 | Preserve timestamps (modified/created for files; separately for dirs), attributes, descriptions, owner, permissions/ACLs, sparse files, ADS | A (mtime/atime, chmod/chown, xattrs/ACLs) | M | Idea |  |
| COPY-06 | Strip "Mark of the Web" | W | S | Dropped | Windows only |
| COPY-07 | **Copy queues**: serialize jobs per device; auto-manage queues; named queues; ghost files for queued copies | L | L | Idea |  |
| COPY-08 | Progress dialogs: speed graph, percent in title, pause/resume/skip/abort, unattended mode, minimize to the jobs bar | L | L | Done v0.2.0 | No jobs bar yet (LST-13); delay, graph and title percentage are fixed, not options |
| COPY-09 | Pre-count files before copy/delete (optionally via an index) | L | S | Idea |  |
| COPY-10 | Error dialog: retry/skip/abort; "skip identical"; merge folders without asking; export error list | L | M | Idea |  |
| COPY-11 | **Make links**: shortcut, symlink (absolute/relative), hard link, junction, auto | L (symlink/hardlink) | M | Idea | The engine makes links already (CLIP-01); this adds the copy commands |
| COPY-12 | Copy filtered (`FILTER`), skip folders, flatten Flat-View copies (single level / recreate / ask) | L | M | Idea |  |
| COPY-13 | Move-when-same-filesystem shortcut; copy-with-Shift-moves | L | S | Idea |  |
| COPY-14 | Non-buffered I/O threshold; buffer size | L (O_DIRECT) | S | Idea |  |
| COPY-15 | Copy into archives; extract from archives on copy | L | L | Idea |  |
| COPY-16 | Copy to a collection (as member or sub-collection) | L | M | Idea |  |
| COPY-17 | Send to email (attach) | A | S | Idea |  |
| COPY-18 | Install fonts | A (~/.local/share/fonts) | S | Idea |  |
| COPY-19 | "Copy of" naming template; duplicate with date/time names; move to a new or dated sub-folder | L | M | Idea |  |
| COPY-20 | Drag-and-drop semantics: drop on files/folders toggle; spring-loaded folders; drag between Listers | L | L | Idea |  |
| COPY-21 | Copy and move are offered by the locked source side's selection, and say so, while the other side is in use | L | S | Done v0.2.0 | Was IDEA-16. Errors show on the side in use |
| COPY-22 | **Drag and drop**: drag files to the other side, a tab, a folder, the folder tree, the location bar, another window or another program. Ctrl copies, Shift moves, Ctrl+Shift links, Alt asks; the status bar says what letting go will do | L | M | Done v0.2.0 | Was IDEA-18. With no key: moves on the same drive, copies between drives and from other programs, or as Edit → When Dropping Files says. Holding files over a tab shows it. For other programs the key must be down as the drag starts (Wayland fixes what a drag offers then). Tested by hand on Hyprland, within CargoHatch and with Flea and Nautilus (2026-11-25). Spring-loaded folders and the rest stay in COPY-20 |

## 10. Delete and recycle (DEL)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| DEL-01 | Delete to Recycle Bin or permanently; Shift reverses | A (freedesktop Trash) | M | Done v0.2.0 |  |
| DEL-02 | Confirmation options: before starting, per file, per folder, read-only files, skip for recycle | L | M | Done v0.2.0 | No "Shift skips confirmation", since Shift means delete for good here |
| DEL-03 | Skip or fail on non-empty folders | L | S | Idea |  |
| DEL-04 | **Secure wipe** delete (N passes); securely empty the recycle bin; securely erase free space (with an SSD warning) | L | M | Idea |  |
| DEL-05 | Empty Recycle Bin; recycle bin toggles | A | S | Idea |  |
| DEL-06 | **Undelete** (restore from the recycle bin) | A | M | Idea |  |
| DEL-07 | Filtered delete | L | S | Idea |  |

## 11. Rename (REN)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| REN-01 | **Inline rename** (F2) with selection modes (name only, all, extension, home/end) and control keys to switch parts | L | M | Done v0.2.0 | F2 cycles name / whole / extension; configurable control keys are IDEA-06 |
| REN-02 | Inline: rename all selected at once; move to next/prev file keeping the cursor; rename into another dir by typing a path; auto-number on clash; suggestion popups | L | M | Idea |  |
| REN-03 | **Advanced Rename dialog** with live **preview list** | L | L | Idea |  |
| REN-04 | Modes: standard wildcard (`*`→`*`), **regex** find/replace, find & replace text, **script** (JS rename function) | L | L | Idea |  |
| REN-05 | Case actions: upper, lower, first word, all words; extension upper/lower | L | S | Idea |  |
| REN-06 | **Numbering** from N by step; auto-number on clash; macros (multi-step recorded rename) | L | M | Idea |  |
| REN-07 | **Metadata-based names** using `{field}` codes (EXIF date, ID3 artist, dimensions…) | L | L | Idea |  |
| REN-08 | Recurse into sub-folders; rename folders as well; rename "matching" files together (same stem) | L | M | Idea |  |
| REN-09 | Presets (save/load/history); Undo rename; paste names from clipboard (one per line) | L | M | Idea |  |
| REN-10 | Ignore extension, case-sensitive match, apply even if no match | L | S | Idea |  |

## 12. Other file commands (FCMD)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FCMD-01 | **Create folder** with a name, path (`a/b/c`), multiple folders at once (one per line), name suggestions, optionally read into a new tab | L | M | Done v0.2.0 | Several at once use brace lists and ranges (`{Jan,Feb}`, `{01..12}`) |
| FCMD-02 | Create a new file (text file, or from a template / "New" menu); create from clipboard contents | L (~/Templates) | M | Idea |  |
| FCMD-03 | **Change attributes and timestamps**: set/clear/toggle attrs, chmod with an owner/group/world grid, set created/modified to a date, shift by an offset, or copy from EXIF taken/digitized/parent dates; recurse with a filter | A (chmod/touch; no creation-time set) | M | Idea |  |
| FCMD-04 | **Descriptions/comments** on files (stored inside the file, in a sidecar `descript.ion`, or in ADS) | A (xattr `user.comment`) | M | Idea |  |
| FCMD-05 | **Labels** (colour/style tags on files): label categories, wildcard/filter-driven auto-labels, folder labels, stored in the filesystem; label column; pin labelled items to the top | L (xattr) | L | Idea |  |
| FCMD-06 | **Tags, ratings and metadata editing**: EXIF, ID3/APE, document props; multi-value tag add; tag history popup; GPS editing (decimal) | L (exiftool/taglib) | L | Idea |  |
| FCMD-07 | **Split file** into chunks (size, count, or disk presets); join; uuencode/base64 encode | L | M | Idea |  |
| FCMD-08 | **Join files** (ordered list, optional decode uudecode/base64/text) | L | M | Idea |  |
| FCMD-09 | **Checksums**: MD5, SHA-1/256/512, CRC32 (+PHP variants), BLAKE3; hash cache; copy hashes to the clipboard | L | M | Idea |  |
| FCMD-10 | **Calculate folder sizes**: on demand, automatically (all/local/fixed/selected), cached, updated after operations; junction handling; per-folder overrides | L | M | Idea |  |
| FCMD-11 | **Image conversion and resize**: convert to JPG/PNG/GIF/BMP/…, quality, resize by size or percent (presets), never enlarge/reduce, rotate, flip, crop, lossless JPEG rotate, add suffix, regex rename, preserve date | L | L | Idea |  |
| FCMD-12 | Set desktop wallpaper (center/tile/stretch/fit/fill/span) | A (DE-specific) | S | Idea |  |
| FCMD-13 | Properties dialog for files and folders | L | M | Idea |  |
| FCMD-14 | **Print folder listing** (to printer, file, or clipboard; columns, flat view, filter, txt/csv/tab, presets) | L | M | Idea |  |
| FCMD-15 | Undo/redo of file operations (copy, move, rename, delete, create), with an undo list and log | L | L | Idea |  |
| FCMD-16 | File operation **log** (copy/move/delete/rename/create; level and size limit) | L | M | Idea |  |
| FCMD-17 | Play sound files (internal player) | L | S | Idea |  |
| FCMD-18 | File type summary dialog (counts and sizes by extension/group) | L | M | Idea |  |
| FCMD-19 | Run "Open with" menu, "Send to" menu, "New" menu | A (xdg MIME apps) | M | Idea |  |
| FCMD-20 | **Open a terminal** in the current folder (Shift+F4, File menu, right-click on empty space), or in a folder from its right-click menu in the list or tree | L | S | Done v0.2.0 | Was IDEA-10. `xdg-terminal-exec` picks the terminal, falling back to `$TERMINAL`; what goes wrong is shown in the status bar. Tested by hand with Alacritty (2026-11-25) |
| FCMD-21 | **Open several selected items** at once, with Enter or the right-click menu: files in their applications, folders in new tabs after the current one | L | S | Done v0.2.0 | Was IDEA-11. Asks first above 10 items. A double-click still opens the one clicked. Tested by hand (2026-11-25) |

## 13. Clipboard (CLIP)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| CLIP-01 | Cut/copy/paste files; paste as link (symlink/hardlink) | L | M | Done v0.2.0 | Paste as link: symbolic, relative, hard |
| CLIP-02 | **Paste clipboard content as a file**: image (jpg/png/gif/bmp with quality), text (UTF-8/16/ANSI), zip/7z | L | M | Idea |  |
| CLIP-03 | **Copy names**: full path, name, no extension, URL, UNC, quoted, WSL path, hashes, custom format | L | S | Done v0.2.0 | Hashes are FCMD-09; UNC and WSL paths don't apply on Linux |
| CLIP-04 | Copy file contents as image or text; copy info tips; copy column data | L | M | Idea |  |
| CLIP-05 | **Screenshot** (window/all, save to file, timer, format, quality, name template) | A (Wayland portal) | M | Idea |  |
| CLIP-06 | Clipboard-change script event | L | S | Idea |  |

## 14. Search, duplicates and sync (FIND)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FIND-01 | **Simple find** in the utility panel: name (wildcard, regex, any word, partial, case, diacritics), containing text, in folders, recursive, search archives | L | L | Idea |  |
| FIND-02 | **Advanced find** using full filter definitions (size, date, attrs, metadata, content) | L | L | Idea |  |
| FIND-03 | Content search options: regex, UTF-8, any word, case, whole word | L | M | Idea |  |
| FIND-04 | Results go to a **collection** (virtual folder), or into the source/dest/left/right/new tab; refine; clear previous | L | M | Idea |  |
| FIND-05 | **Indexed search engines**: Everything, Windows Search; "Everything" for folder sizes and counts | A (plocate/recoll/tracker/baloo) | M | Idea |  |
| FIND-06 | Saved find presets; repeat last; exclusions; skip hidden/system | L | M | Idea |  |
| FIND-07 | **Duplicate file finder**: compare by name, size, date, MD5 (full or partial 25/50/75%), MD5 cache, column-based; min size; ignore hard links; number groups | L | L | Idea |  |
| FIND-08 | **Duplicate selection helper**: select all but first, by preferred locations, checkbox mode | L | M | Idea |  |
| FIND-09 | **Synchronize folders**: one-way or two-way, compare by size/date/newer/content, delete extras (first, to trash), ignore seconds, DST compensation, sub-folders, filters, preview list with conflicts | L | L | Idea |  |
| FIND-10 | Find empty / zero-byte folders | L | S | Idea |  |

## 15. Collections, libraries and virtual folders (COLL)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| COLL-01 | **File collections**: persistent virtual folders of references to files anywhere; sub-collections; add/remove; date-added column | L | L | Idea |  |
| COLL-02 | Collections appear in the tree, on the desktop, and in "Send to" | A | M | Idea |  |
| COLL-03 | **Virtual folders built natively**: This PC (drives grouped by type, free-space warnings, size units), Desktop, Network computers, Recycle Bin, Libraries, Quick Access | A (drives page, trash:, network via gvfs/smb) | L | Idea |  |
| COLL-04 | "Treat virtual folders as real" settings | W | M | Dropped | Windows only |

## 16. Archives and VFS (ARC)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| ARC-01 | **Browse into archives as folders** (zip built-in; 7z, rar and others via plugins), read-only by default, show in tree, hide extensions from tree | L (libarchive) | L | Idea |  |
| ARC-02 | **Create archive**: type, compression level, AES 128/192/256 encryption, password, split volumes, full paths, single-folder removal, set archive date to newest file, self-extracting, multiple archives at once | L (SFX is Windows-only) | L | Idea |  |
| ARC-03 | Copy into an existing zip (add); ask for encryption/compression on copy; temp-file safety for removable drives | L | M | Idea |  |
| ARC-04 | Extract (here, to sub-folder, checkout); batch extract; **auto-extract and run** on double-click (installers) | L | L | Idea |  |
| ARC-05 | Archive context menu (Add, Add to named, Archive and Email, Extract, cascade) | L | M | Idea |  |
| ARC-06 | Zip comments (view/edit) | L | S | Idea |  |
| ARC-07 | Archive entry properties (packed size, ratio, CRC, method) | L | S | Idea |  |
| ARC-08 | VFS plugin manager (enable/disable, configure, order); plugin API | L (our own plugin ABI) | L | Idea |  |
| ARC-09 | MTP devices and portable devices browsing | A (gvfs-mtp) | M | Idea |  |

## 17. FTP / SFTP and network (NET)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| NET-01 | **FTP/FTPS/SFTP browsing** in a normal file display (copy, delete, rename, etc.) | L (libssh2/curl or gio) | L | Idea |  |
| NET-02 | **FTP address book**: sites, folders, per-site settings (host, port, anon, auth, key file, initial dir, time-zone and DST adjust) | L | L | Idea |  |
| NET-03 | Connection settings: PASV, timeouts, retry/reconnect, keep-alive, site-to-site (FXP), data port range | L | M | Idea |  |
| NET-04 | Server command toggles: MLST, MDTM, FEAT, RESUME, UTF8, custom LIST | L | M | Idea |  |
| NET-05 | Transfer mode ASCII/binary/auto with extension list | L | S | Idea |  |
| NET-06 | Proxy (FTP proxy, SOCKS 4/5, HTTP) | L | M | Idea |  |
| NET-07 | Bandwidth limits for upload/download with schedules | L | M | Idea |  |
| NET-08 | Password caching/saving, global/anonymous password | L (Secret Service/keyring) | M | Idea |  |
| NET-09 | Index-file downloading (ls-lR style) | L | S | Idea |  |
| NET-10 | FTP log panel (find, save, copy, float) | L | M | Idea |  |
| NET-11 | **Edit remote file → re-upload when changed** (with checksum detection) | L | M | Idea |  |
| NET-12 | Network drive credentials (`Go NETUSER/NETPASS`) | A (smb via gvfs) | M | Idea |  |
| NET-13 | Send files by email via SMTP (SSL/TLS, auth) | L | M | Idea |  |

## 18. Viewer, preview and QuickShow (VWR)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| VWR-01 | **Viewer pane** preview of images, text, documents, audio, video, hex | L | L | Done (unreleased) | Images (animated too, big ones scaled as read), text (UTF-8 or Latin-1, the first 512 KB), a hex dump of anything else (the first 64 KB), the first page of PDFs with poppler's pdftoppm, audio and video with Qt Multimedia, and folder item counts. Read on worker threads. Office documents are VWR-10 |
| VWR-02 | **Standalone image viewer**: zoom modes, fit, fullscreen, next/prev in folder, slideshow (fade, random, speed), rotate, flip, crop, save-as, gamma, checkerboard for alpha, histogram/info overlay, magnifier | L | L | Idea |  |
| VWR-03 | Lister-linked viewer (follows the selection, both ways); reuse the existing viewer | L | M | Idea |  |
| VWR-04 | **Marked pictures**: mark images in the viewer to build a list, then send it to a collection | L | M | Idea |  |
| VWR-05 | **QuickShow**: press Space for an instant popup preview, dismiss on key release, frameless | L | M | Idea |  |
| VWR-06 | Viewer mouse configuration (edge-click next/prev, wheel actions) | L | S | Idea |  |
| VWR-07 | Text viewer with hex mode, find/replace, go to line, UTF-8 detection | L | L | Idea |  |
| VWR-08 | Image formats: JPEG, PNG, GIF (animated), BMP, PCX, TGA, WebP, JPEG2000, camera RAW (LibRaw/dcraw settings), PNM; external converter commands for unknown formats | L | L | Idea |  |
| VWR-09 | Audio tags and waveform thumbnails; movie thumbnails/preview | L (ffmpeg) | M | Idea |  |
| VWR-10 | Document preview (Office/PDF via a plugin) | A (poppler; LibreOffice optional) | L | Idea | The first page of a PDF shows in the viewer pane (VWR-01); Office documents still to do |
| VWR-11 | Text-file thumbnails | L | S | Idea |  |
| VWR-12 | Viewer pane exclusions by folder | L | S | Idea |  |
| VWR-13 | Map lookup of GPS coordinates (online services) | L | S | Idea |  |
| VWR-14 | The viewer pane refreshes when the file it shows changes on disk, and moves on when it's deleted | L | S | Idea | Came up building VWR-01 |
| VWR-15 | Zoom with the wheel, and rotate or flip images, in the viewer pane | L | S | Idea | Came up building VWR-01 |
| VWR-16 | Video shows its first frame before it's played, and the pane can play media by itself | L | S | Idea | Came up building VWR-01 |

## 19. Customization: toolbars, menus, hotkeys and commands (CUST)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| CUST-01 | **Fully editable toolbars and menus** (drag commands in Customize mode): buttons, menus, sub-menus, three-button groups, fields, separators | L | L | Idea |  |
| CUST-02 | Toolbar positions: top, bottom, left, right, tree, viewer pane, location bar, **floating** (auto-hide, dock to screen edge, appbar) | A (Wayland floating limits) | L | Idea |  |
| CUST-03 | **Toolbar sets** and per-view-mode toolbars (toolbar changes with the view mode); per-folder toolbars | L | L | Idea |  |
| CUST-04 | Button appearance: label/icon/both, colours, fonts, image sizes, icon sets, backgrounds | L | M | Idea |  |
| CUST-05 | **Command editor**: function types (internal command, external/shell, script), run with logging, multi-line functions | L | L | Idea |  |
| CUST-06 | **Internal command language**: about 30 commands with typed arguments, plus `{file}`-style **external control codes** (`{f}`, `{filepath$}`, `{allfilepath}`, `{o}`, `{s}`, `{d}`…), modifiers (`@confirm`, `@async`, `@nodeselect`…) | L | L | Idea |  |
| CUST-07 | **User commands**: define new named commands with templates and arguments, usable like built-ins | L | M | Idea |  |
| CUST-08 | **Hotkeys**: Lister keys, viewer keys, folder-tree keys, **system-wide** hotkeys; conflict detection; import/export; key search | A (global keys need portal/compositor) | L | Idea |  |
| CUST-09 | Configurable **mouse actions**: double-click and middle-click per modifier (file display, tree), Power-mode buttons | L | M | Idea |  |
| CUST-10 | **Context menus** fully editable (file display bg, collection bg, archive bg, tab bar, column headers, tray icon, viewer) | L | L | Idea |  |
| CUST-11 | Shell-extension manager: allow/block context-menu handlers, out-of-process, hide third-party items | W | M | Idea |  |
| CUST-12 | **Launch options** for programs dropped on buttons (send files, per file or single line, async, start-in, test harness) | L | M | Idea |  |
| CUST-13 | Format Painter (copy button styling) | L | S | Idea |  |
| CUST-14 | Customize-dialog command catalogue (categories, drag to toolbar), toolbar glossary | L | M | Idea |  |
| CUST-15 | **Right-click menu** for files, folders and empty space (open, cut, copy, paste, rename, delete, create folder…), fixed for now | L | M | Done v0.2.0 | Was IDEA-07. CUST-10 makes it editable |
| CUST-16 | **Menu bar sorted by what it holds**: File acts on items (Open, Open in New Tab, Open on Other Side, Rename, Delete…), Edit copies, moves, selects (Select submenu) and finds, View shows, Go moves, a **Tabs** menu holds every tab command including lock and colour, Folder Aliases sit under Favourites, and a **Settings** menu gathers every choice that stays set | L | M | Done v0.2.0 | Was IDEA-20 to IDEA-27. Every action is also the window's own, so keys work from any submenu. Reopen Last Closed heads the Recently Closed lists. The source/destination lock reads "Source and Destination ▸ Follow the Active Side / Keep This Side as…" so it isn't confused with a tab's lock. The preferences window (CFG-01) can replace Settings later |
| CUST-17 | **Keyboard Shortcuts window** (Help menu, F1, or ? in the file list): every key, read from the menus, plus the file list's and dragging's own, with a search field; stays open beside the window | L | S | Done v0.2.0 | Was IDEA-28 |

## 20. File types (FTYP)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FTYP-01 | File type editor: per-extension and per-group actions (double-click, Ctrl/Shift/Alt-double-click, drag-drop, context-menu items), icons, descriptions, info-tip and tile definitions | L (layer over shared-mime-info) | L | Idea |  |
| FTYP-02 | **File type groups** (Images, Music, Movies, Documents, Archives…) used in filters and formats | L | M | Idea |  |
| FTYP-03 | Special pseudo-types: All files, All folders, Drives, No extension, Flat View item, Library item | L | M | Idea |  |
| FTYP-04 | Internal viewer/player overrides on double-click; open unknown plain text in the text viewer | L | S | Idea |  |

## 21. Scripting, Evaluator and automation (SCR)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| SCR-01 | **Script add-ins** (one language, e.g. JavaScript via QuickJS, Lua or Python) that add commands, columns, Quick-Key commands and event handlers | L | L | Idea |  |
| SCR-02 | **Script events**: start and shutdown, windows and tabs opening, closing and switching, folder changes, clicks, file system changes, finished operations, renames, custom columns and commands, timers, clipboard and settings changes | L | L | Idea |  |
| SCR-03 | Script management dialog (status, version, commands/columns/events per script, include files, packages `.osp`, install from file) | L | L | Idea |  |
| SCR-04 | Script-defined dialogs (resource editor) | L | M | Idea |  |
| SCR-05 | **Evaluator**: a built-in expression language used in filters, columns, grouping, labels, quick filter and button labels | L | L | Idea |  |
| SCR-06 | **Evaluator columns** (user-defined computed columns with type, graph, stars, caching, grouping, markup) | L | L | Idea |  |
| SCR-07 | **CLI / ad-hoc script editor** (run commands or scripts interactively, with output) | L | M | Idea |  |
| SCR-08 | External control: a command-line tool that sends commands to the running instance | A (D-Bus or Unix socket) | L | Idea |  |
| SCR-09 | Scheduled and periodic timers in scripts | L | S | Idea |  |
| SCR-10 | Script log panel; logs with date/time/ms prefixes | L | S | Idea |  |
| SCR-11 | Custom **status bar** definitions with codes (selection counts, sizes, free space, format lock icon…) plus custom codes | L | M | Idea |  |

## 22. Favourites, recent and history (FAV)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| FAV-01 | **Favourites** list (tree with branches, sort, separators); Favourites Bar; add dialog | L | L | Done v0.2.0 | Also in the folder tree, and files as well as folders |
| FAV-02 | **SmartFavorites**: automatically learned frequently-used folders, with thresholds and exclusions | L | M | Idea |  |
| FAV-03 | Recent folders list (size, include virtual, ignore transit folders) | L | M | Idea |  |
| FAV-04 | Privacy: disable or clear history categories | L | S | Idea |  |
| FAV-05 | Jump list / tray menu (recent and favourite items) | A (StatusNotifierItem) | M | Idea |  |

## 23. Folder tree (TREE)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| TREE-01 | Tree styles, indent, item height, full-row select, root separators, header | L | M | Done v0.2.0 | Drive label styles left out: Linux mounts are named by their folder |
| TREE-02 | Highlight the path to the selection and to all open tabs (with tab colour) | L | M | Idea |  |
| TREE-03 | Content: hidden folders, system folders, archives, virtual folders; remove dynamically added | L | M | Done v0.2.0 | Archives split out as TREE-09; virtual folders beyond the trash are COLL-03 |
| TREE-04 | Auto-expand to the current folder; collapse non-selected branches (unless open in other tabs); remember expansion in layouts/tabs | L | M | Done v0.2.0 |  |
| TREE-05 | **Expansion pins** (permanently expanded branches) | L | S | Idea |  |
| TREE-06 | Spring-loaded expansion while dragging | L | S | Idea |  |
| TREE-07 | Tree lock (don't follow navigation) | L | S | Idea |  |
| TREE-08 | Breadcrumb-style root items (This PC, Network, Cloud) | A | M | Idea |  |
| TREE-09 | Archives in the folder tree | L | S | Idea | Split from TREE-03; needs ARC-01 |

## 24. Look and feel (LOOK)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| LOOK-01 | **Themes**: bundles of colours, fonts, images and blending; import/export, partial apply, backup before apply | L | L | Idea |  |
| LOOK-02 | Colours and fonts for every element (file colours by type/attribute, selection opacities, inactive-pane dimming, progress, graphs) | L | L | Idea |  |
| LOOK-03 | Colour blending (row/column/background) | L | M | Idea |  |
| LOOK-04 | Icon sets (import, order, disable) | L | M | Idea |  |
| LOOK-05 | Sound events (operation complete, errors) | L | S | Idea |  |
| LOOK-06 | Language/translation system | L | M | Idea |  |
| LOOK-07 | **Menus outlined in the theme accent**, with visible separator lines, so an open menu stands apart from the window behind it | L | S | Done v0.2.0 | Drawn by the app style, so every menu and right-click menu has it and it follows a theme change |

## 25. Configuration and program (CFG)

| ID | Feature | Fit | Size | Status | Notes |
|---|---|---|---|---|---|
| CFG-01 | Preferences dialog with search; several hundred pages (all of the above) | L | L | Idea |  |
| CFG-02 | **Backup/restore configuration** (selective, encrypted, with description); **export to USB** (portable install) | L | L | Idea |  |
| CFG-03 | Startup: launch on login; reopen last Listers or a layout; periodic auto-backup of open Listers; startup delay | A (XDG autostart) | M | Idea |  |
| CFG-04 | Tray icon with configurable double-click and menu | A | S | Idea |  |
| CFG-05 | Global hotkey to open a Lister (Win+E replacement) | A | S | Idea |  |
| CFG-06 | Desktop double-click and desktop context-menu integration | W/A | M | Idea |  |
| CFG-07 | Shared vs private configuration per user | L | M | Idea |  |
| CFG-08 | Auto-shutdown when all functions finish; system shutdown/sleep after a job (`Close SYSTEM`, `WHENFINISHED`) | A (systemd) | M | Idea |  |
| CFG-09 | Update checker, news | L (our own) | S | Idea |  |
| CFG-10 | Task manager for CargoHatch's own background tasks; diagnostics and crash logs | L | M | Idea |  |
| CFG-11 | Licensing and certificates | W (not wanted) | S | Dropped | Not wanted: CargoHatch is free |
| CFG-12 | Logging: file function log, FTP/script log limits | L | S | Idea |  |
| CFG-13 | Mouse/keyboard behaviours: Alt-click to edit buttons, Alt key toolbar mode, tooltips, single-click mode | L | M | Idea |  |
| CFG-14 | Cloud storage awareness (sync status columns, allow download of placeholders) | A | M | Idea |  |
| CFG-15 | External tools: text viewer, script editor, external image formats | L | M | Idea |  |
| CFG-16 | **About window** with the logo, version and licence | L | S | Done v0.2.0 | Was IDEA-15. Under Help. Credits the user and Claude |

---

## Suggested order (a design choice; the user decides)

1. **First real release, "a usable dual-pane manager":** every feature chosen for it is built. Publishing it needs a public repo and a tarball-based PKGBUILD for omarchy-pkgs.
2. **Power user:** COPY-07 (queues), REN-03..09 (advanced rename), FIND-01/02/04, FIND-09 (sync), FIND-07 (dupes), FCMD-03/09/10, VIEW-14 (Flat View), VIEW-15 (grouping), VIEW-25 (frozen columns), FMT-01..05, SEL-05, LST-24 (NavLock).
3. **Archives and preview:** ARC-01..04, VWR-01/02/05, VIEW-08/10, FCMD-11.
4. **Customization:** CUST-01/05/06/07/08/10, SCR-11 (status bar), LOOK-01/02.
5. **Scripting and automation:** SCR-01/02/05/06/07/08.
6. **Remote:** NET-01/02/03/08/11.
7. **The rest:** labels and metadata (FCMD-05/06), collections, themes, backup.

## Done by release

- **v0.1.0** (pre-release, 2026-11-21): LST-01/02/03/04/06/07/19/20 (in part)/21/26 (in part), TAB-01/02/04/05/07/08/09/10/11/12/13/15/18/19, NAV-01/02
- **v0.2.0** (first public release, 2026-11-27): VIEW-01, VIEW-02 (in part), SORT-01/02/03, COPY-01/02/03/08, DEL-01/02, SEL-01/02, REN-01, FCMD-01, CLIP-01/03, FILT-01/03, SEL-11, CUST-15, NAV-03/07, LST-08, TREE-01/03/04, FAV-01, CFG-16, LST-34, COPY-21/22, FCMD-20/21, NAV-08/18, LST-35, CUST-16/17, LOOK-07
- **Unreleased:** LST-09, VWR-01

---

## Appendix: column ideas

- **File basics:** name, size, type, modified, created, accessed, attributes, extension, description, owner, full path, path length, parent folder, link target, status, size on disk, relative size, relative dates, folders
- **Pictures:** width, height, bit depth, dimensions, date taken, camera make and model, lens, aperture, shutter speed, ISO, white balance, exposure, focal length, flash, metering
- **Audio and video:** title, artists, album, album artist, year, track, disc, genre, BPM, duration, bit rates, sample rate, codecs, frame rate, languages and stream counts, HDR, encoder
- **Documents:** authors, title, subject, category, pages, comments, created, last saved, last edited by, creating program, protected
- **Programs:** description, version, product name, company, copyright
- **Hashes:** MD5, SHA-1, SHA-256
- **Other:** font name, physical size, group
