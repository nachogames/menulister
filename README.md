# MenuLister 🍔

A powerful macOS utility for programmatically exploring and interacting with application menu bars! 🚀

## What is MenuLister? 🤔

MenuLister is a command-line tool that allows you to:
- 📋 List all menu items from any macOS application
- 🎯 View keyboard shortcuts, enabled/disabled states, and other menu item properties
- 🤖 Programmatically trigger menu actions
- 🔍 Navigate through complex menu hierarchies

## Usage 💻

```bash
# List all menus for an application
menulister <pid>

# Trigger a specific menu action
menulister <pid> --action "Menu/Submenu/Item"
```

### Examples 🌟

```bash
# List all menus for application with PID 1234
menulister 1234

# Open Developer Tools in an application
menulister 1234 --action "Help/Toggle Developer Tools"
```

## Requirements ⚡

- macOS
- Accessibility permissions (will be requested on first run)

## Status 🚧

This project is currently a work in progress. Features and improvements are being added regularly!

---

Made with ❤️ and 🤖 