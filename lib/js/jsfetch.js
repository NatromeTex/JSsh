function jsfetch() {
  function repeat(s, n) {
    let out = '';
    for (let i = 0; i < n; i++) out += s;
    return out;
  }

  function padRight(s, n) {
    return s + repeat(' ', n - s.length);
  }

  // ANSI colors
  const CLR_RESET = "\x1b[0m";
  const CLR_LABEL = "\x1b[38;2;85;154;211m";  // soft blue
  const CLR_VALUE = "\x1b[38;2;220;220;170m"; // faded yellow

  const logo = [
    "       __ ____ ____ __  __",
    "      / / ___/ ___// / / /",
    " __  / /\\__ \\\\__ \\/ /_/ /",
    "/ /_/ /___/ /__/ / __  / ",
    "\\____//____/____/_/ /_/ ",
    "No Home like 127.0.0.1"
  ];

  function getSysInfo() {
    let username = sys.username() || "user";
    let cwd = sys.getcwd();
    let tty = sys.ttyname(0) || "tty0";
    let cols = 80, rows = 24;
    let cpu = sys.getcpu();
    let ram = sys.getram();
    let pkg = sys.getpkgCount();
    let ver = version()
    try {
      let ws = sys.getWinSize();
      if (ws && ws.length === 2) {
        cols = ws[0];
        rows = ws[1];
      }
    } catch {}
    return { username, cwd, tty, cols, rows, cpu, ram, pkg, ver};
  }

  const info = getSysInfo();
  const infoLines = [
    `${CLR_LABEL}Version:${CLR_RESET}\t${CLR_VALUE}${info.ver}${CLR_RESET}`,
    `${CLR_LABEL}User:${CLR_RESET}\t${CLR_VALUE}${info.username}${CLR_RESET}`,
    `${CLR_LABEL}CPU:${CLR_RESET}\t${CLR_VALUE}${info.cpu.model} ${CLR_RESET}`,
    `${CLR_LABEL}Compute:${CLR_RESET}\t${CLR_VALUE}${info.cpu.cores} cores ${info.cpu.threads} threads${CLR_RESET}`,
    `${CLR_LABEL}Memory:${CLR_RESET}\t${CLR_VALUE}${info.ram} MiB${CLR_RESET}`,
    `${CLR_LABEL}CWD: ${CLR_RESET}\t${CLR_VALUE}${info.cwd}${CLR_RESET}`,
    `${CLR_LABEL}TTY: ${CLR_RESET}\t${CLR_VALUE}${info.tty}${CLR_RESET}`,
    `${CLR_LABEL}Size:${CLR_RESET}\t${CLR_VALUE}${info.cols}x${info.rows}${CLR_RESET}`,
    `${CLR_LABEL}Packages:${CLR_RESET}\t${CLR_VALUE}${info.pkg}${CLR_RESET}`
  ];

  const maxLogoWidth = Math.max(...logo.map(l => l.length));
  const totalLines = Math.max(logo.length, infoLines.length);

  for (let i = 0; i < totalLines; i++) {
    const logoLine = logo[i] || '';
    const infoLine = infoLines[i] || '';
    printR(padRight(logoLine, maxLogoWidth + 2) + infoLine + "\n");
  }
}
