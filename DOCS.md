# JSSH OS Primitive Commands

This document lists the built-in OS-level commands exposed in the JSSH REPL. All commands are implemented in **native C** and callable from the REPL. They do **not** invoke an external shell unless explicitly noted.

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
## `update()`

* **Description**: Rebuilds the JSSH binary using `make` in the current folder and replaces the running REPL process with the newly built binary. History is preserved across the update.
* **Parameters**: None.
* **Example**:

```js
update()
```

* **Behavior**:

  1. Saves the current readline history to `~/.jssh_history`.
  2. Runs `make` in the current directory.
  3. Replaces the current process with the newly built binary.
  4. Reloads history in the new REPL automatically.

---

### Notes

* All commands are implemented **natively in C** using POSIX/syscalls.
* No shell invocation occurs for these commands (`system()` is only used internally in `update()` for convenience).
* These primitives form the foundation for building more complex shell-like scripts in JSSH.
