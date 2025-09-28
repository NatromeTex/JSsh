# JSSH OS Primitive Commands

This document lists the built-in OS-level commands exposed in the JSSH REPL. Most commands are implemented in **native C** and callable from the REPL. They do **not** invoke an external shell unless explicitly noted.

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

## `net.ping()`

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

### Notes
* No shell invocation occurs for these commands (`system()` is only used internally in `update()` for convenience).
* These primitives form the foundation for building more complex shell-like scripts in JSSH.
* Installation of net-utils needs sudo on your system.
