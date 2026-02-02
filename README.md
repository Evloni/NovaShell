# NovaShell

A lightweight, feature-rich command shell written in C. NovaShell provides a modern terminal experience with built-in commands, environment variable management, script execution, and more.

## Features

- **Built-in Commands**: Essential shell commands including `cd`, `pwd`, `echo`, `export`, `clear`, and `help`
- **Environment Variable Management**: Full support for setting, exporting, and expanding environment variables
- **Script Execution**: Execute shell scripts (.sh files) and any executable with shebang
- **Command History**: Persistent command history with up/down arrow navigation
- **Tab Completion**: Auto-completion for built-in commands
- **Colorful Interface**: Beautiful color-coded output with customizable themes
- **Cross-Platform**: Built on standard C libraries for wide compatibility

## Installation

### Prerequisites

- GCC compiler
- Standard C libraries (unistd.h, stdlib.h, etc.)
- Make (optional, for building)

### Building from Source

1. Clone or download the NovaShell source code
2. Navigate to the project directory
3. Compile the shell:

```bash
make compile
```

Or manually:

```bash
gcc -Wall -Wextra src/main.c src/utils.c src/linenoise.c -o nsh -Isrc/libs
```

4. Run NovaShell:

```bash
./nsh
```

## Usage

### Basic Usage

Start NovaShell by running the compiled binary:

```bash
./nsh
```

You'll see the NovaShell prompt:

```
nsh — Nova Shell
nsh v1.0.0
Type `help` to show available commands!

nsh $ 
```

### Built-in Commands

#### `exit`
Exit the shell.

```bash
nsh $ exit
```

#### `pwd`
Print the current working directory.

```bash
nsh $ pwd
/home/user/projects
```

#### `cd [directory]`
Change the current directory.

```bash
nsh $ cd /path/to/directory
nsh $ cd ..          # Go up one directory
nsh $ cd             # Go to home directory
```

#### `echo [text]`
Print text to the terminal with variable expansion support.

```bash
nsh $ echo "Hello, World!"
Hello, World!

nsh $ echo "Current user: $USER"
Current user: username

nsh $ echo "Home directory: ${HOME}"
Home directory: /home/username
```

#### `export [VAR[=value]]`
Manage environment variables.

```bash
# Set a new variable
nsh $ export MY_VAR=hello

# Export an existing variable
nsh $ export PATH

# List all environment variables
nsh $ export
declare -x HOME=/home/user
declare -x PATH=/usr/bin:/bin
declare -x MY_VAR=hello
```

#### `clear`
Clear the terminal screen.

```bash
nsh $ clear
```

#### `help`
Show help information for all built-in commands.

```bash
nsh $ help
  exit                    Exit the shell
  pwd                     Print current working directory
  cd <directory>          Change directory
  export                  List all environment variables
  export VAR=value        Set and export environment variable
  export VAR              Export existing variable
  echo [text]             Print text (supports $VAR expansion)
  clear                   Clear the screen
  help                    Show this help message
```

### Script Execution

NovaShell can execute shell scripts in two ways:

#### 1. Direct Script Execution
Run a script file directly:

```bash
nsh $ ./myscript.sh
nsh $ ./myscript.sh arg1 arg2
```

#### 2. Command Line Script Execution
Execute a script as the first argument to NovaShell:

```bash
nsh $ ./nsh myscript.sh
nsh $ ./nsh myscript.sh arg1 arg2
```

### Environment Variables

NovaShell supports full environment variable management:

#### Setting Variables
```bash
nsh $ export MY_VAR=value
nsh $ export PATH=/usr/local/bin:$PATH
```

#### Variable Expansion in Echo
```bash
nsh $ echo "User: $USER, Home: $HOME"
User: username, Home: /home/username

nsh $ echo "Full path: ${HOME}/Documents"
Full path: /home/username/Documents
```

### Command History

NovaShell maintains a persistent command history:
- Use up/down arrow keys to navigate through previous commands
- History is saved to `history.txt` in the current directory
- History is loaded automatically when NovaShell starts

### Tab Completion

NovaShell provides tab completion for built-in commands:
- Type the beginning of a command and press Tab
- Available completions will be shown
- Works for all built-in commands: `exit`, `cd`, `echo`, `export`, `clear`, `help`, `pwd`

## Color Scheme

NovaShell uses a carefully designed color scheme:

- **Prompt/Commands**: Bright cyan (`#64C8FF`)
- **Regular Output**: Light gray/white (`#E6E6E6`)
- **Success Messages**: Bright green (`#64FF64`)
- **Warnings**: Bright yellow/orange (`#FFC864`)
- **Errors**: Bright red (`#FF6464`)
- **Info Text**: Soft blue (`#96C8FF`)

## Project Structure

```
NovaShell/
├── src/
│   ├── main.c              # Main shell implementation
│   ├── utils.c             # Utility functions and command handlers
│   ├── linenoise.c         # Line editing library
│   └── libs/
│       ├── utils.h         # Header file with function declarations
│       └── linenoise.h     # Line editing library header
├── Makefile               # Build configuration
├── README.md              # This documentation file
├── LICENSE                # GNU GPLv3 license
└── THIRD_PARTY_LICENSES.txt  # Third-party license information
```

## Development

### Building
```bash
make compile    # Compile the shell
make clean      # Remove compiled binary
make run        # Compile and run
```

### Adding New Commands
To add new built-in commands:

1. Add the command name to the `commands` array in `completion()` function in `utils.c`
2. Add command handling logic in the main loop in `main.c`
3. Update the help text in the `help` command

### Contributing
Contributions are welcome! Please follow these guidelines:
- Maintain the existing code style
- Add appropriate error handling
- Update documentation for new features
- Test thoroughly before submitting

## License

NovaShell is my own code and is licensed under the **GNU GPLv3** (see LICENSE).

NovaShell uses the following third-party libraries:

- **linenoise** (BSD 2-Clause License)  
  Full license text included in THIRD_PARTY_LICENSES.txt

If you redistribute NovaShell or its binaries, please preserve all
copyright notices and license texts for third-party libraries.
