// Help database: all docs live here
const helpdb = {
    clear: `
{green}clear(){reset}
  Clear the terminal screen.
`,

    cat: `
{green}cat(path){reset}
  Print the contents of a file.

  Example:
    cat("file.txt")
`,
    tac: `
{green}tac(path){reset}
  Print the contents of a file in reverse order.

  Example:
    tac("file.txt")
`,

    echo: `
{green}echo(value){reset}
  Print a value to stdout.

  Example:
    echo("Hello World")
`,

    ls: `
{green}ls(path, flag){reset}
  List directory contents.
  Flags:
    "l"  show long format with permissions, size, timestamp.

  Example:
    ls("/etc", "l")
`,

    cd: `
{green}cd(path){reset}
  Change the current working directory.

  Example:
    cd("/tmp")
`,

    mkdir: `
{green}mkdir(path, mode){reset}
  Create a directory with optional mode (default 0755).

  Example:
    mkdir("newdir", 755)
`,

    touch: `
{green}touch(path){reset}
  Create a new empty file or update its timestamp.

  Example:
    touch("file.txt")
`,

    rm: `
{green}rm(path, flags){reset}
  Remove files or directories.
  Flags:
    "f"   force
    "r"   recursive
    "rf"  recursive + force

  Example:
    rm("mydir", "rf")
`,

    chmod: `
{green}chmod(path, octal){reset}
  Change file permissions.

  Example:
    chmod("file.txt", 644)
`,

    js: `
{green}js(path){reset}
  Run a JavaScript file inside the shell.

  Example:
    js("script.js")
`,

    date: `
{green}date(){reset}
  Print the current date and time.
`,

    show_env: `
{green}show_env(){reset}
  Show current environment configuration.
`,

    update: `
{green}update(){reset}
  Rebuild and restart the shell using 'make'.
`
};

// Unified help function
function help(cmd) {
    if (!cmd) {
        printR("{yellow}Available commands:{reset}\n");
        for (let name in helpdb) {
            // Grab first doc line after signature
            let lines = helpdb[name].trim().split("\n");
            let summary = lines[1] ? lines[1].trim() : "";
            printR(`  {green}${name}{reset} - ${summary}\n`);
        }
        printR("\nType help(\"command\") for detailed help.\n");
    } else {
        if (helpdb[cmd]) {
            printR(helpdb[cmd]);
        } else {
            printR(`{red}No help found for '${cmd}'{reset}\n`);
        }
    }
}
