# EmailViews

A native fast, lightweight email client for [Haiku](https://www.haiku-os.org) that uses live queries to organize and explore your emails effortlessly.

EmailViews works with Haiku's built-in `mail_daemon` and mail kit — it reads emails already stored on disk as file attributes, so there is nothing to import or sync. Just point it at your mail and go.

![EmailViews screenshot](screenshot.jpeg)

EmailViews was created using AI tools and is maintained by Jorge Mare.

## Features

**Three-pane interface** — Sidebar with mail views, sortable email list, and inline preview pane. Double-click any email to open it in a full reader window.

**Built-in views** — All Emails, Unread, Sent, With Attachments, Drafts, and Starred. Each view shows an unread count badge that updates in real time.

**Custom queries** — Create your own views filtered by subject, sender, recipient, or account. Custom queries are saved as standard Haiku query files and appear in the sidebar alongside built-in views.

**Live updates** — Email views are powered by live `BQuery` objects and node monitors. New mail appears instantly without manual refresh.

**Full reader and composer** — Open emails in a dedicated reader window with reply, forward, and signature support. Compose new messages with address auto-completion from People files and spell checking.

**Attachment handling** — Visual attachment strip shows file icons, names, and sizes. Open attachments with their preferred application or save them to disk. Supports drag-and-drop.

**HTML email support** — Emails with HTML content can be viewed in the default browser via a button in the preview pane, preserving the original formatting and character encoding.

**Search and filter** — Search the current view by subject, sender, or recipient. An optional time range slider lets you narrow results to a specific date window.

**Starred emails** — Star important emails with a single click. Stars are stored as a file attribute (`FILE:starred`) so they persist across sessions and are queryable.

**Sortable columns** — Click any column header to sort by status, star, attachment, sender/recipient, subject, date, or account. Drag columns to reorder them. Column layout is saved per view.

**Keyboard navigation** — Arrow keys, Page Up/Down, Home/End for list navigation. Enter to open, Delete to trash. Shift-click and Shift-arrows for multi-selection.

**Trash management** — Move emails to trash, restore them to their original location, or permanently delete them. The trash view shows a count badge and supports emptying all at once.

**Email backup** — Back up the current view's emails to a ZIP archive via the toolbar search bar's menu.

**Deskbar integration** — An optional Deskbar replicant shows the unread mail count in the system tray with a popup menu for quick access.

**Multi-volume support** — Query emails across multiple mounted volumes.

**Dark theme support** — Respects Haiku's system colors and works with both light and dark themes.

**Localization** — The app is localization ready.

## Requirements

- 64-bit Haiku (tested on R1/nightly builds)
- At least one email account configured in Haiku's E-mail preferences
- `mail_daemon` running (starts automatically when email accounts are configured)

## Building

EmailViews uses Haiku's standard makefile engine:

```sh
make
```

To build a release version:

```sh
make OPT_NOASSERT=1
```

The resulting `EmailViews` binary is placed in the current directory (or under `objects.*-release/` depending on your build configuration).

## Installation

Copy the `EmailViews` binary anywhere you like — `/boot/home/apps/` is a common choice. No additional files are needed; all resources (icons, translations) are embedded in the binary.

To launch, double-click the binary or run it from Terminal:

```sh
EmailViews
```

On first run, EmailViews creates a `queries` directory in its settings folder (`~/config/settings/EmailViews/queries/`) to store any custom queries you create.

## Usage tips

- **Creating custom queries**: Right-click an email in the list and choose "Add 'From' query", "Add 'To' query", or "Add 'Account' query" from the Messages menu to create a filtered view for that sender, recipient, or account.
- **Time range filtering**: Press Cmd+Shift+T to toggle the time range slider, which lets you narrow results to a specific date range.
- **Starring emails**: Click the star column in the email list, or use the toolbar button in the reader window.
- **Deskbar replicant**: Enable via the EmailViews menu → "Show in Deskbar". The tray icon shows the current unread count.
- **Column customization**: Drag column headers to reorder, click to sort. Each view remembers its own column layout.

## Credits

EmailViews is built on the shoulders of Haiku's mail kit and draws inspiration from several Haiku applications:

- **Haiku Mail** (Mail application by the Haiku Project) — reader/composer foundation
- **Beam** by Oliver Tappe — attribute search UI and attachment handling patterns
- **QuickLaunch** by Humdinger — Deskbar replicant integration
- **Tracker** by the Haiku Project — file management patterns

Icons from the Haiku and Zumi icon sets.

Special thanks to **Humdinger** for meticulous testing, detailed bug reports, and valuable feature suggestions.

## License

Distributed under the terms of the MIT License.
