# Modern Notepad

A sleek, dark-themed tabbed text editor for Windows built with pure Win32 API. Features a modern UI with tabbed document interface, real-time text statistics, and a minimalist design.

![Modern Notepad Screenshot](https://via.placeholder.com/800x500?text=Modern+Notepad+Screenshot)

## Features

- **Tabbed Interface** - Open and manage multiple documents in a single window with an intuitive sidebar tab system
- **Dark Mode** - Modern dark theme with carefully chosen colors for comfortable editing
- **Real-time Statistics** - Live word count, character count, and token estimation in the status bar
- **Tab Management** - Create new tabs, close tabs, and rename tabs with ease
- **File Operations** - Open and save text files with Unicode support
- **Lightweight** - Built with pure Win32 API, no external dependencies required
- **Fast & Native** - Compiled to native code with minimal memory footprint

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| New Tab | Click "+ New" button |
| Open File | Click "Open" button |
| Save File | Click "Save" button |
| Rename Tab | Right-click on tab |
| Close Tab | Click "×" on tab |
| Switch Tab | Left-click on tab |

## Building

### Prerequisites

- MinGW-w64 cross-compiler (or any Windows C++ compiler)
- Windows SDK (included with MinGW-w64)

### Compile with MinGW-w64

```bash
x86_64-w64-mingw32-g++ -o ModernNotepad.exe main.cpp \
    -mwindows -municode -static \
    -lgdi32 -lcomctl32 -luxtheme -ldwmapi -lole32 -lcomdlg32 \
    -O2 -std=c++17
```

### Compile with MSVC

```bash
cl /EHsc /O2 /std:c++17 ModernNotepad.cpp \
    /link user32.lib gdi32.lib comctl32.lib uxtheme.lib dwmapi.lib ole32.lib comdlg32.lib
```

## Usage

1. **Creating a New Tab** - Click the "+ New" button in the toolbar or the new tab button
2. **Opening a File** - Click "Open" to browse and open a text file
3. **Saving a File** - Click "Save" to save the current tab (prompts for location if unsaved)
4. **Renaming a Tab** - Right-click on any tab to rename it (works for both saved and unsaved tabs)
5. **Closing a Tab** - Click the "×" button on the tab to close it

## Technical Details

- **Language**: C++17
- **API**: Win32 API
- **UI Framework**: Native Windows controls with custom rendering
- **Font**: Consolas (monospace) for the editor, Segoe UI for UI elements
- **Encoding**: UTF-16 LE with BOM support for file I/O

## Project Structure

```
ModernNotepad/
├── main.cpp           # Main source file
├── README.md          # This file
├── LICENSE            # MIT License
└── ModernNotepad.exe  # Compiled executable
```

## Color Scheme

| Element | Color | RGB |
|---------|-------|-----|
| Window Background | Dark Gray | RGB(32, 32, 32) |
| Active Tab | Medium Dark | RGB(42, 42, 42) |
| Editor Background | Slightly Lighter | RGB(45, 45, 45) |
| Active Tab Indicator | Cyan | RGB(96, 205, 255) |
| Text | White | RGB(255, 255, 255) |
| Inactive Text | Gray | RGB(160, 160, 160) |

## Known Limitations

- Only supports UTF-16 LE encoding for saving files
- No syntax highlighting
- No line numbers
- No find/replace functionality
- Tab names are independent of file names after saving

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests for:

- Bug fixes
- New features
- UI improvements
- Code optimization

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built with the Windows API
- Inspired by modern code editors like VS Code and Sublime Text
- Dark mode implementation uses DWMWA_USE_IMMERSIVE_DARK_MODE attribute