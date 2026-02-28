// Help database, now structured by category.
// All information is self-contained.
const helpdb = {
    "Core commands": {
        _note: "Core commands built into JSSH.",
        "sys.sudo": `
{green}sys.sudo(){reset}
  Runs JSSH with elevated privileges. Use with caution.
`,
        "clear": `
{green}clear(){reset}
  Clears the terminal screen.
`,
        "cat": `
{green}cat(path){reset}
  Prints the contents of a file to stdout.

  Example:
    cat("file.txt")
`,
        "tac": `
{green}tac(path){reset}
  Prints the contents of a file in reverse order.

  Example:
    tac("file.txt")
`,
        "echo": `
{green}echo(value){reset}
  Prints the provided value to stdout.

  Example:
    echo("Hello, World!")
`,
        "ls": `
{green}ls(path, flag){reset}
  Lists the contents of a directory.
  - path (string, optional): Directory to list. Defaults to "./".
  - flag (string, optional): "l" for long listing format.

  Example:
    ls("/etc", "l")
`,
        "cd": `
{green}cd(path){reset}
  Changes the current working directory.

  Example:
    cd("/usr/local")
`,
        "mkdir": `
{green}mkdir(path, mode){reset}
  Creates a new directory.
  - mode (integer, optional): Permissions for the new directory. Defaults to 0755.

  Example:
    mkdir("new_dir", 0700)
`,
        "touch": `
{green}touch(path){reset}
  Creates an empty file or updates the modification time.

  Example:
    touch("file.txt")
`,
        "rm": `
{green}rm(path, [flags...]){reset}
  Removes a file or directory.
  - flags (string, optional): "r" for recursive, "f" for force.

  Example:
    rm("old_dir", "rf")
`,
        "chmod": `
{green}chmod(path, octal){reset}
  Changes the permissions of a file using octal notation.

  Example:
    chmod("script.js", 755)
`,
        "js": `
{green}js(path){reset}
  Executes a JavaScript file in the JSSH environment.

  Example:
    js("my_script.js")
`,
        "date": `
{green}date(){reset}
  Prints the current date and time.
`,
        "show_env": `
{green}show_env(){reset}
  Shows the current environment settings for JSSH.
`,
        "update": `
{green}update([Modules]){reset}
  Rebuilds and restarts the JSSH binary.
  Preserves command history across the update.

  Example:
    update("network") // Rebuild with the network module
    update("network", "compiler", "fs") // Rebuild with the network, compiler and file system module
`
    },
    "Net Utils": {
        _note: "To install, run update('network') with sudo privileges.",
        "net.ssh": `
{green}net.ssh(host){reset}
  Opens an SSH session to a remote host.

  Example:
    net.ssh("user@192.168.1.100")
`,
        "net.ping": `
{green}net.ping(IP){reset}
  Pings a given IP address to check connectivity.

  Example:
    net.ping("1.1.1.1")
`,
        "net.netstat": `
{green}net.netstat(){reset}
  Displays current network connections and listening ports.
`,
        "net.ifconfig": `
{green}net.ifconfig(){reset}
  Displays network interface configurations.
`,
        "net.tracert": `
{green}net.tracert(IP){reset}
  Traces the network route to a destination host.

  Example:
    net.tracert("google.com")
`,
        "net.route": `
{green}net.route(){reset}
  Displays the Kernel IP routing table.
`
    },
    "Crypt Utils": {
        _note: "A pure JS library. Ensure crypt.js is present in /libs/js to access.",
        "crypt.sha1": `
{green}crypt.sha1("msg"){reset}
  Calculates the SHA-1 hash of a string.
`,
        "crypt.sha256": `
{green}crypt.sha256("msg"){reset}
  Calculates the SHA-256 hash of a string.
`,
        "crypt.sha384": `
{green}crypt.sha384("msg"){reset}
  Calculates the SHA-384 hash of a string.
`,
        "crypt.sha512": `
{green}crypt.sha512("msg"){reset}
  Calculates the SHA-512 hash of a string.
`,
        "crypt.md5": `
{green}crypt.md5("msg"){reset}
  Calculates the MD5 hash of a string.
`,
        "crypt.hmac": `
{green}crypt.hmac(algo, msg, key){reset}
  Generates a Hash-based Message Authentication Code (HMAC).

  Example:
    crypt.hmac("sha256", "message", "secret_key")
`,
        "crypt.base64": `
{green}crypt.base64("msg", mode){reset}
  Base64 encodes or decodes a string.
  - mode (int): 0 to encode, 1 to decode.

  Example:
    crypt.base64("SGVsbG8=", 1) // returns "Hello"
`,
        "crypt.byteDump": `
{green}crypt.byteDump(path){reset}
  Returns the byte content of a file as a string. Useful for hashing files.

  Example:
    crypt.sha256(crypt.byteDump("./jssh"))
`,
        "crypt.randomBytes": `
{green}crypt.randomBytes(length){reset}
  Generates cryptographically strong random bytes (Linux only).

  Example:
    crypt.randomBytes(16)
`,
        "crypt.compare": `
{green}crypt.compare(algo, target, hash){reset}
  Compares a target string against a hash to prevent timing attacks.

  Example:
    crypt.compare("sha256", "password", "hash_to_check")
`
    },
    "Compiler Utils": {
        _note: "To install, run update('compiler') with sudo privileges.",
        "cmp.list": `
{green}cmp.list(){reset}
  Lists all compilers installed on system.

  Example:
    cmp.list()
`,
        "cmp.<compiler>": `
{green}cmp.<compiler>("path"){reset}
  Uses the specified compiler to compile (and run) the specified file.

  Example:
    cmp.python3("test.py")
`,
        "cmp.auto": `
{green}cmp.auto("path"){reset}
  Determines the best compiler based on file extension and runs the suitable compiler. The required compiler must be installed on the system.

  Example:
    cmp.auto("program.go")
`
    },
    "File System Utils": {
        _note: "To install, run update('fs') with sudo privileges.",
        "fs.tree": `
{green}fs.tree("path"){reset}
  Displays the directory tree for the specified path.

  Example:
    fs.tree("./home")
`,
        "fs.find": `
{green}fs.find("path", {flags}){reset}
  Recursively traverses the path specified to find the file specified.

  Example:
    fs.find("/bin", { name: "*.c", type: "f"})
`,
        "fs.df": `
{green}fs.df(){reset}
  Prints the disk space usage of all connected and configured disks.

  Example:
    fs.df()
`
    },
    "Git Utils": {
        _note: "To install, run update('git') with sudo privileges.",
        "git.init": `
{green}git.init("path"){reset}
  Initializes a Git repository at the specified path.
  
  Example:
    git.init("path")
`,
        "git.clone": `
{green}git.clone("repo_url", "path"){reset}
  Clones a Git repository to the specified path.
  
  Example:
    git.clone("https://github.com/user/repo.git", "./repo")
`,
        "git.config": `
{green}git.config(key[, value]){reset}
  Gets or sets a local Git config value in the current repository.

  Examples:
    git.config("user.name")
    git.config("user.email", "me@example.com")
`,
        "git.add": `
{green}git.add(path){reset}
  Stages changes for commit. Use "." to stage all tracked/untracked files.

  Examples:
    git.add("file.txt")
    git.add(".")
`,
        "git.status": `
{green}git.status(){reset}
  Shows a short status of the working tree (similar to 'git status -s').

  Example:
    git.status()
`,
        "git.commit": `
{green}git.commit(message){reset}
  Creates a commit from the current index. Uses user.name and user.email from git config if available.

  Example:
    git.commit("Initial commit")
`,
        "git.diff": `
{green}git.diff(){reset}
  Shows a patch-style diff between HEAD, index, and working tree.

  Example:
    git.diff()
`,
        "git.branch": `
{green}git.branch()[reset]
  Lists local branches.

{green}git.branch(name){reset}
  Creates a new branch at HEAD.

  Examples:
    git.branch()
    git.branch("feature-x")
`,
        "git.checkout": `
{green}git.checkout(name){reset}
  Checks out an existing local branch.

  Example:
    git.checkout("main")
`,
        "git.merge": `
{green}git.merge(name){reset}
  Merges the given local branch into the current HEAD (simple merges only; conflicts are reported as errors).

  Example:
    git.merge("feature-x")
`,
        "git.pull": `
{green}git.pull([remote]){reset}
  Fetches from the given remote (default "origin") and merges the corresponding remote branch into the current branch.

  Examples:
    git.pull()
    git.pull("origin")
`,
        "git.fetch": `
{green}git.fetch([remote]){reset}
  Fetches from the given remote (default "origin") without merging, updating remote-tracking branches.

  Example:
    git.fetch()
`,
        "git.push": `
{green}git.push([remote]){reset}
  Pushes the current branch to the given remote (default "origin"). Uses SSH agent or default Git credentials when available.

  Example:
    git.push()
`,
        "git.log": `
{green}git.log([n]){reset}
  Shows the last n commits on the current branch (default 10). Prints abbreviated hash, author and subject.

  Examples:
    git.log()
    git.log(20)
`,
        "git.stash": `
{green}git.stash([message]){reset}
  Saves the current working tree and index state to a new stash entry.
  Prints a note if there are no local changes to stash.

  Examples:
    git.stash()
    git.stash("WIP: refactor")
`
    },
    "Applications": {
        _note: "To install, run update('apps') with sudo privileges.",
        "app.jsvim": `
{green}app.jsvim(){reset}
  Launches the JSVim text editor application.

  Example:
    app.jsvim()
`
    }
};

// Unified help function, updated to handle categories
function help(cmd) {
    if (!cmd) {
        printR("{yellow}Available commands:{reset}\n");

        for (const category in helpdb) {
            printR(`\n{cyan}--- ${category} ---{reset}\n`);
            if (helpdb[category]._note) {
                printR(`  (${helpdb[category]._note})\n`);
            }
            for (const name in helpdb[category]) {
                if (name === "_note") continue; // Skip the note key
                
                // Grab first doc line after signature
                const lines = helpdb[category][name].trim().split("\n");
                const summary = lines[1] ? lines[1].trim() : "No summary available.";
                const paddedName = `{green}${name}{reset}`.padEnd(32, ' ');
                printR(` ${paddedName} - ${summary}\n`);
            }
        }
        printR("\nType help(\"command\") for detailed help.\n");
    } else {
        let found = false;
        for (const category in helpdb) {
            if (helpdb[category][cmd]) {
                printR(helpdb[category][cmd]);
                found = true;
                break;
            }
        }
        if (!found) {
            printR(`{red}No help found for '${cmd}'{reset}\n`);
        }
    }
}