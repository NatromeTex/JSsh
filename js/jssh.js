// jssh.js
// Bootstrap helpers for JSSH

// shell runner provided from C side
globalThis.sh = function (cmd) {
    return __shell__(cmd); // __shell__ is your C binding
};

// print function for convenience
globalThis.print = function (...args) {
    for (let a of args) {
        std.out.puts(String(a));
    }
    std.out.puts("\n");
};

// if you want TypeScript support (when TS compiler is loaded):
globalThis.tsEval = function (code) {
    // assuming tsc.js is loaded in ctx
    let transpiled = ts.transpileModule(code, { compilerOptions: { module: ts.ModuleKind.CommonJS }});
    return eval(transpiled.outputText);
};

// greet
print("JSSH runtime loaded. Mode = shell by default. Use :js to switch.");
