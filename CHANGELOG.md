# Changelog

## Unreleased

- A viewer pane (F11, or View → Viewer Pane) previews the item under the
  cursor: images, including animated ones; text; a hex dump of other files;
  the first page of a PDF, when poppler is installed; audio and video, with
  a play button and a position slider; and how many items are in a folder.
  It sits beside or below the files, can be resized, and remembers all that
  with the window and in layouts. Its padlock keeps a file in view while you
  move on, and its arrows step through the list.
- Rename Several (Shift+F2, File menu or right-click) finds and replaces in
  the selected names and changes their capitalisation, with a preview of
  every new name. It can match case, leave extensions out, and change the
  capitalisation even where Find doesn't match. It never renames over
  another item, and stops names that would clash or become hidden.
- Settings → Viewer Pane can keep the last item in view when nothing is
  selected, and wrap long lines of text.

## 0.2.0 (2026-11-27)

The first public release. CargoHatch was two sides of tabs in 0.1.0; it is
now a file manager you can use all day.

### Seeing your files
- Seven view modes: large icons, small icons, list, details, power,
  thumbnails and tiles (Ctrl+1 … Ctrl+7).
- Twelve columns in the details views, which you can show, hide, reorder and
  resize. Each tab keeps its own view mode, columns and sort.
- Sort by any column, with folders first, last or mixed, and numbers sorted by
  value (file2 before file10).
- The list updates by itself as files come and go.
- Hidden files stay hidden until you press Ctrl+H. You can also hide names by
  pattern, and names listed in a folder's `.hidden` file.

### Working with files
- Copy and move to the other side (F5, F6), to a folder you choose (Shift+F5,
  Shift+F6), or duplicate in place.
- When a name is taken, a window shows both items side by side and lets you
  replace, skip, resume or rename. You can also choose one answer for every
  time, under Settings.
- Big jobs show a progress window with a speed graph and time left, and can
  be paused, skipped or left to run unattended.
- Drag and drop to the other side, a tab, a folder, the folder tree, the
  location bar, another window or another program. Ctrl copies, Shift moves,
  Ctrl+Shift links and Alt asks.
- Cut, copy and paste that work with other file managers, paste as links
  (symbolic, relative or hard), and copy names or paths as text.
- Delete moves to the trash; Shift+Delete deletes for good, after asking.
- Rename in place with F2. Pressing F2 again selects the whole name, then the
  extension.
- Create a folder, a path of folders, or many at once (F7, `Disc {01..12}`).
- Enter opens every selected item: files in their applications, folders in
  new tabs. Shift+F4 opens a terminal in the current folder.
- A right-click menu for files, folders and empty space.

### Selecting and finding
- Type in the list to jump to a name. Every match is highlighted and marked
  on the scroll bar; F3 and Shift+F3 move between them.
- The filter bar (Ctrl+F) shows only the names that match, by part of a name,
  wildcards or a regular expression.
- Select all, files only, folders only, none, or the opposite; select by
  pattern (Ctrl+S); and get back the selection from before the last command.

### Getting around
- A folder tree (F9) for the whole window or for each side, with Home, the
  file system, drives, network mounts and the trash. It follows the folder
  you're in.
- Favourite folders and files, in the Favourites menu, the tree and a
  favourites bar, sorted into branches if you like (Ctrl+B adds one).
- Paths complete as you type in the location bar.
- Folder aliases such as `~downloads`, `~config` and `~trash`, plus your own,
  wherever you type a path.
- Going up puts the cursor on the folder you came from.
- Typing or passing a file's path opens its folder with the file selected,
  including on the command line.

### The window
- The menus are sorted by what they do: File, Edit, View, Go, Tabs,
  Favourites, Window, Settings and Help. Settings holds the choices that
  stay set.
- Help → Keyboard Shortcuts (F1, or ? in the file list) lists every key, with
  a search field. It stays open beside the window, so you can try the keys.
- Menus are outlined in the theme's accent colour, so an open menu stands
  apart from the window behind it.
- An About window, under Help.
- A window locked as the destination copies and moves from its paired source
  window, and copy and move follow a side locked as the source.
- Errors in the status bar clear on the next key or click, or after eight
  seconds.

## 0.1.0 (2026-11-21)

The first pre-release: one or more windows, each with one or two sides of
tabs and a source and destination; tab locking, linking, colours, names,
home folders and groups; saved layouts; reopening closed tabs and windows;
and back, forward and up with a location bar of clickable folders.
