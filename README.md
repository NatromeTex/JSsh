# JSsh

> JavaScript is God’s programming language. Why not use it as your shell too?

`jssh` is a REPL shell built on [QuickJS](https://bellard.org/quickjs/).  
It gives you a Unix-style shell prompt, but everything you type is JavaScript.  
Need `cat`? It’s a JS function. Want `echo`? Same deal.  

Because who doesn’t want their shell to throw exceptions instead of exit codes?

---

## Features

- Run JavaScript interactively at a prompt.  
- Unix commands are now JS functions.  
- Supports as many colors as your terminal provides.  
- Extensible: drop JS files into a commands directory and extend your shell.  
- Written in C, powered by QuickJS.  
- Zero shame in declaring JavaScript the one true language.

---

## Installation

1. Clone this repo:

   ```bash
   git clone https://github.com/yourname/jssh.git
   cd jssh
   ```
    Get the QuickJS source files (version 2020-11-08 recommended).
    Extract them into /src so you have the quickjs files in /src

2. Build with make:
   ```bash
    make
   ```
   This will compile everything and produce the binary at bin/jssh.

3. Usage

   Run the shell:
    ```bash
    ./bin/jssh
    ```

   You will get a prompt like:

   `username@hostname:/current/directory$`

   From here you can type JavaScript directly:
      
   ```js
   1 + 2
   // => 3
   
   echo("hello world")
   // prints "hello world"
   ```
   Use `CTRL+D` to exit.

## Development

All code is present in `/src`, entry is in main.c <br>
Built-ins like cat and echo live in utils.c or func.c. <br>
Some base level access like read and write file need to be given to JS using C<br>
Extend by adding more JS-callable functions in C, or by loading .js modules. <br>

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).
You are free to run, study, share, and modify this software, but derivative works must also be licensed under the GPL.

See the full text in LICENSE

## Closing Words

Use jssh.
Because a TypeError stack trace at the prompt is way more fun than command not found.

Built with ❤️