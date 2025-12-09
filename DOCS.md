# JSSH OS Primitive Commands

This document lists the built-in OS-level commands exposed in the JSSH REPL. Most commands are implemented in **native C** and callable from the REPL. They do **not** invoke an external shell unless explicitly noted.

---
## `sys.sudo()`

* **Description**: Runs `JSsh` with elevated privileges. Under development so be **careful** of actions performed while in running `JSsh` with root privileges.
* **Parameters**: None
* **Example**:

```js
sys.sudo()
```

---

## `clear()`

* **Description**: Clears the screen.
* **Parameters**: None
* **Example**:

```js
clear()
```

---

## `cat(path)`

* **Description**: Prints the contents of a file to stdout.
* **Parameters**:

  * `path` (string): Path to the file to read.
* **Example**:

```js
cat("file.txt")
```

---

## `tac(path)`

* **Description**: Prints the contents of a file to stdout in reverse order.
* **Parameters**:

  * `path` (string): Path to the file to read.
* **Example**:

```js
tac("file.txt")
```

---

## `echo(value)`

* **Description**: Prints the provided value to stdout.
* **Parameters**:

  * `value` (any): Value to print. Converted to string if necessary.
* **Example**:

```js
echo("Hello, World!")
```

---

## `ls(path, flag)`

* **Description**: Lists the contents of a directory.
* **Feature**: Color codes by file/directory type.
* **Parameters**:

  * `path` (string, optional, default `"./"`): Directory path to list.
  * `flag` (string, optional): `"l"` for long listing (permissions, size, modification time).
* **Examples**:

```js
ls()              // short listing of current directory
ls("/etc")        // short listing of /etc
ls("/etc", "l")   // long listing of /etc
```

---

## `cd(path)`

* **Description**: Changes the current working directory.
* **Parameters**:

  * `path` (string): Target directory path.
* **Example**:

```js
cd("/usr/local")
pwd()             // returns "/usr/local"
```

---

## `mkdir(path, mode)`

* **Description**: Creates a new directory.
* **Parameters**:

  * `path` (string): Directory path to create.
  * `mode` (integer, optional, default `0755`): Permissions for the new directory.
* **Examples**:

```js
mkdir("test")       // default permissions
mkdir("foo", 0700)  // custom permissions
```

---

## `touch(path)`

* **Description**: Creates an empty file if it does not exist, or updates the modification time if it does.
* **Parameters**:

  * `path` (string): File path to touch.
* **Example**:

```js
touch("file.txt")
```

---

## `rm(path, [flags...])`

* **Description**: Removes a file or directory. Supports optional flags.
* **Parameters**:

  * `path` (string): Path to remove.
  * `flags` (string, optional): `"r"` for recursive, `"f"` for force (ignore errors).
* **Examples**:

```js
rm("file.txt")          // remove file
rm("file.txt", "f")     // force remove
rm("dir", "r")          // remove directory recursively
rm("dir", "rf")         // remove directory recursively and force
```

---

## `chmod(path, octal)`

* **Description**: Changes the permissions of the specified file. Requires an int number in the standard unix octals.
* **Parameters**:

  * `path` (string): Path to file whose permissions need to be modified.
  * `octal` (int): Permissions in numeric notation.
* **Examples**:

```js
chmod("file.txt", 755)          // grant read, write and execute to owner and read and execute to others
chmod("file.txt", 444)          // file read-only for all
```

---
## `js()`

* **Description**: Similar to `sh` command in normal shell. Executes a js file in JSsh, the js file being executed has access to all the above functions and any function defined using JS in `/js/lib`
* **Parameters**:
  
  * `path` (string): Path to js file which needs to be executed.
* **Examples**:

```js
// script.js
echo("Hello from script.js!");
ls(".", "l");
echo("Files above ");

function greet(name) {
    return "Hi " + name + "!";
}

echo(greet("world"));

// in JSsh
js("script.js")
```
---
## `date()`

* **Description**: Prints out a simple date-time string.
* **Parameters**: None
* **Example**:
```js
date()
// It is Sun, Aug 31st, 2025 00:03:31 GMT+0000
```
This is the first pure JS command added to `JSsh`.

---
## `show_env()`

* **Description**: Shows the current environment settings for `JSsh`.
* **Parameters**: None
* **Example**:

```js
show_env()
```

---
## `update()`

* **Description**: Rebuilds the JSSH binary using `make` in the current folder and replaces the running REPL process with the newly built binary. History is preserved across the update.
* **Parameters**:
  * `Modules` (string): Includes the specified modules from `/lib/*` based on name. Type `help(update)` to see available modules. 
* **Example**:

```js
update([Modules])
```

* **Behavior**:

  1. Saves the current readline history to `~/.jssh_history`.
  2. Runs `make` in the current directory.
  3. Replaces the current process with the newly built binary.
  4. Reloads history in the new REPL automatically.


<br>

# Net Utils commands

This section details the commands present in the Net Utils package available installing the **network** package. All code in this package is written in **native C** and does not depend on `system()` to run the code in the JSsh REPL.

---

## `net.ssh(host)`

* **Description**: Opens an SSH session to the host defined in the connection string.
* **Parameters**: 
  * `host` (string): Connection string of the remote machine. Can be in the format `user@host` or `user@host:port`.
* **Example**:

```js
net.ssh("user@192.168.1.2")
// Connecting to user@192.168.1.2
// The server is unknown. Do you trust the host key?
// Public key hash: AA:BB:CC:DD:EE:FF::XX:YY:ZZ [yes/NO]: yes
// user@192.168.1.2's password: 
//
// user@192.168.1.2:~$
```

---

## `net.ping(IP)`

* **Description**: Pings the given IP address and returns responses.
* **Parameters**: 
  * `IP` (string): IP address of the server to be pinged.
* **Example**:

```js
net.ping("1.1.1.1")
```

---

## `net.netstat()`

* **Description**: Displays the current established, inbound/outbound and disconnected network connections.
* **Parameters**: None
* **Example**:

```js
net.netstat()
```

---

## `net.ifconfig()`

* **Description**: Displays the current interfaces and their IP addresses along with additional info.
* **Parameters**: None
* **Example**:

```js
net.ifconfig()
//lo:  Link encap:Ethernet  HWaddr 00:00:00:00:00:00
//      inet addr:127.0.0.1  Bcast:0.0.0.0  Mask:255.0.0.0
//      Flags:0x49  MTU:65536
```

---

## `net.tracert(IP)`

* **Description**: Displays the hops between your system and the specified IP/Domain. Performs dns and reverse dns lookups where necessary.
* **Parameters**:
  * `IP` (string): IP address or Domain name of server to which the route has to be traced.
* **Example**:

```js
net.tracert("google.com")
//Tracing route to google.com [142.250.77.110]
//over a maximum of 30 hops:

//  1     0 ms     0 ms     0 ms  172.31.16.1
//  2     3 ms     3 ms     3 ms  X.X.X.X [X.X.X.X]
//  3    17 ms    17 ms    17 ms  Y.Y.Y.Y [Y.Y.Y.Y]
//  4    41 ms    41 ms    41 ms  Z.Z.Z.Z [Z.Z.Z.Z]
//  5    28 ms    28 ms    28 ms  A.A.A.A [A.A.A.A]
//  6    41 ms    41 ms    41 ms  B.B.B.B [B.B.B.B]
//  7     *        *        *     Request timed out.
//  8     *        *        *     Request timed out.
//  9    45 ms    45 ms    45 ms  142.251.55.238 [142.251.55.238]
// 10    44 ms    44 ms    44 ms  216.239.56.65 [216.239.56.65]
// 11    29 ms    29 ms    29 ms  pnmaaa-aq-in-f14.1e100.net [142.250.77.110]

//Trace complete.
```

---

## `net.route()`

* **Description**: Displays the Kernel IP routing tables.
* **Parameters**: None
* **Example**:

```js
net.route()
```

<br>

# Crypt Utils commands

This section details the commands present in the crypt package available through the js lib. All code in this package is written in **native JS** and does not depend on external libraries to run the code in the JSsh REPL.

---

## `crypt.sha1("msg")`

* **Description**: Calculates and displays the sha1 hash of the provided message.
* **Parameters**:

  * `msg` (string): String of which hash has to be calculated.
* **Example**:

```js
crypt.sha1("Hello")
// F7FF9E8B7BB2E09B70935A5D785E0CC5D9D0ABF0
```

---

## `crypt.sha256("msg")`

* **Description**: Calculates and displays the sha256 hash of the provided message.
* **Parameters**:

  * `msg` (string): String of which hash has to be calculated.
* **Example**:

```js
crypt.sha256("Hello")
// 185F8DB32271FE25F561A6FC938B2E264306EC304EDA518007D1764826381969
```

---

## `crypt.sha384("msg")`

* **Description**: Calculates and displays the sha384 hash of the provided message.
* **Parameters**:

  * `msg` (string): String of which hash has to be calculated.
* **Example**:

```js
crypt.sha384("Hello")
// 3519FE5AD2C596EFE3E276A6F351B8FC0B03DB861782490D45F7598EBD0AB5FD5520ED102F38C4A5EC834E98668035FC
```

---
## `crypt.sha512("msg")`

* **Description**: Calculates and displays the sha512 hash of the provided message.
* **Parameters**:

  * `msg` (string): String of which hash has to be calculated.
* **Example**:

```js
crypt.sha512("Hello")
// 3615F80C9D293ED7402687F94B22D58E529B8CC7916F8FAC7FDDF7FBD5AF4CF777D3D795A7A00A16BF7E7F3FB9561EE9BAAE480DA9FE7A18769E71886B03F315
```

---

## `crypt.md5("msg")`

* **Description**: Calculates and displays the md5 hash of the provided message.
* **Parameters**:

  * `msg` (string): String of which hash has to be calculated.
* **Example**:

```js
crypt.md5("Hello")
// 8B1A9953C4611296A827ABF8C47804D7
```

---

## `crypt.hmac(algo, msg, key)`

* **Description**: Provides Hash-based Message Authentication Code for the specified key and message.
* **Parameters**:

  * `algo`   (string): Algorithm to be used.
  * `msg`    (string): Message to be hashed.
  * `key`    (string): Key which is used to authenticate.
* **Example**:

```js
crypt.hmac("sha256", "World", "Hello")
// 59168E309F2C97DD04E45BE3E79BD9ACB6D22FDA6546C00C539282C41EEB916E
```

---


## `crypt.base64("msg", mode)`

* **Description**: Calculates and displays the base64 encoding or decoding of the provided message.
* **Parameters**:

  * `msg` (string): String of which has to be base64 encoded/decoded.
  * `mode`   (int): 0 is encode, 1 is decode.
* **Example**:

```js
crypt.base64("Hello", 0)
// SGVsbG8=
crypt.base64("SlNTSCBpcyBjb29s", 1)
// JSSH is cool
```

---

## `crypt.byteDump(path)`

* **Description**: Dumps the bytes of the file at the path specified. Used to hash files
* **Parameters**:

  * `path` (string): Path of file that needs to be hashed.
* **Example**:

```js
crypt.sha512(crypt.byteDump("./bin/jssh"))
// B3DE8B0DFB9981AAA913EB01EA37F24C4DD1A1E35E397971B41337057025CCE531CBD43B5DFCC714FCDF0B14276EAB9553AB6EBB4A1F6097A4F901A605F731B5
```

---

## `crypt.randomBytes(length)`

* **Description**: Provides the number of random bytes specified by `length` from `/dev/urandom`. (linux only) 
* **Parameters**:

  * `length` (number): Number of bytes to be calculated.
* **Example**:

```js
crypt.randomBytes(10)
// 72,6,221,165,22,92,40,145,140,47
```

---

## `crypt.compare(algo, target, hash)`

* **Description**: Compares the hash of the target given by the algorithm to the hash provided.
* **Parameters**:

  * `algo`   (string): Algorithm to be used.
  * `target` (string): Target to be checked.
  * `hash`   (string): Hash against which target is checked.
* **Example**:

```js
crypt.compare("md5", "Hello", "8B1A9953C4611296A827ABF8C47804D7")
// true
```
<br>

# Compiler Utils commands

This section details the commands present in the compiler package available by installing the **compiler** module. All code in this package is written in **native C** and does not depend on external libraries to run the code in the JSsh REPL. It does require the compilers be installed on the system and registered in `$PATH` to be visible to the detection logic.

---
**Supported compilers:** Python, Python3, gcc, g++, clang, javac, rustc, go, node.

---
## `cmp.list()`

* **Description**: Displays a list of all compilers detected by JSsh and their versions.
* **Parameters**: None
* **Example**:

```js
cmp.list()
// python3: Python 3.12.3
// gcc: gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
// g++: g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
```

---
## `cmp.<compiler_name>("path, flags")`

* **Description**: Runs the specified compiler with the given path and flags.
* **Parameters**:

  * `path` (string): Path at which file to be compiled is present.
  * `flags` (string): Other flags which can be passed to the compiler
* **Example**:

```js
cmp.python3("./hello.py")
// Hello, World!
cmp.gcc("./Home/User/Desktop/main.c -o main")
// !$ ./main
// Hello, World!
cmp.javac("../Test.java")
// !$ java ../Test
// System.out.println is working!
```
---
## `cmp.auto("path")`

* **Description**: Auto-detects the best compiler for the given file and compiles it, reports back if the compiler required for the file is not found.
* **Parameters**: 
    
  * `path` (string): Path to file to be compiled
* **Example**:

```js
cmp.auto("hello.py")
// Hello, World!
cmp.auto("/Home/User/Desktop/main.c -o main")
// !$ ./main
// Hello, World!
cmp.auto("Test.java")
// !$ java ../Test
// System.out.println is working!
```
<br>

# File System Utils commands

This section details the commands present in the File-System package by installing the **fs** module. All code in this package is written in **native C** and does not depend on external libraries to run the code in the JSsh REPL.

---
## `fs.tree("path")`

* **Description**: Displays a tree of all directories and subdirectories in the given path.
* **Parameters**:

  * `path` (string): Path from which tree is called.
* **Example**:

```js
fs.tree("./lib")
// .
// compiler
// ├── cmp_utils.h
// ├── cmp_utils.o
// ├── module.c
// ├── module.o
// └── cmp_utils.c
// js
// ├── jsfetch.js
// ├── crypt.js
// ├── help.js
// └── Date.js
// fs
// ├── module.o
// ├── module.c
// ├── fs_utils.c
// ├── fs_utils.o
// └── fs_utils.h
// network
// ├── net_utils.o
// ├── net_utils.h
// ├── net_utils.c
// ├── module.o
// └── module.c
```
---
## `fs.find("path", {flags})`

* **Description**: Searches through the given directory recursively till the specified file is found.
* **Parameters**: 

  * `path` (string): Directory from which search will be started.
  * `flags` (JSON): Flags to help enhance the search.
    * `name` (string): Uses Fnmatch to search for names, wild-card characters are allowed.
    * `type` (char): '`f`': File only | '`d`': Directory only.
    * `minsize` (int): Skip files below this size.
    * `depth` (int): The subdirectory depth to which the function will search.
* **Example**:

```js
fs.find(".", { name: "main.c", type: "f"})
// ./src/main.c

fs.find("./lib", { name: "*.c", type: "f", depth: 2})
// ./compiler/cmp_utils.c,./compiler/module.c,./fs/fs_utils.c,./fs/module.c,./network/module.c,./network/net_utils.c
```

<br>

# Applications

JSSH ships with a few applications which are standalone and install directly to `usr/local/bin` and are available in bash and JSSH. These apps are the next step in JSSH development.

---
## `apps.jsvim("path")`

* **Description**: Text editor for JSSH, uses intuitive controls rather than vim-style controls. Use to open, read, edit and save code and text files.
* **Parameters**:

  * `path` (string | optional): Path to file to be edited.

* **Example**:

``` js
apps.jsvim("Makefile")
//opens the editor window from JSSH
```

Also works with bash
```bash
#!/bin/bash
jsvim ./test.c
# opens the editor window from bash
```

* **Controls**:

  * Cursor Movement: JSVIM uses arrow keys (Up ↑, Down ↓, Left ←, Right →) for cursor movement
  * Modes: JSVIM starts in `INSERT` mode by default, the `ESC` key switches between the `INSERT` and `COMMAND` modes.
  * INSERT mode: Allows for text to be edited within the editing window.
  * COMMAND mode: Allows you to enter commands, your cursor is locked to the `COMMAND` window. The `COMMAND` mode auto-inserts a ':' at the start because all commands start with a ':' so it is redundant to type :).

* **Commands**:

  | Commands      | What it does                                         |
  |     :---:     |       :---:                                          |
  | :q            | Quits the application without saving buffer          |
  | :w            | Writes the modified buffer to the file               |
  | :wq           | Writes the modified buffer and quits the application |

<br>

### Notes
* No shell invocation occurs for these commands (`system()` is only used internally in `update()` and `sys.sudo()` for convenience).
* These primitives form the foundation for building more complex shell-like scripts in JSSH.
* Installation of net-utils needs sudo on your system.
