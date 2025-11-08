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
  const CLR_LABEL = "\x1b[38;2;90;180;220m";  // soft blue
  const CLR_VALUE = "\x1b[38;2;150;230;150m"; // faded yellow-green

  const logo = [
    "                          ",
    "                          ",
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
    return { username, cwd, tty, cpu, ram, pkg, ver};
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
    `${CLR_LABEL}Packages:${CLR_RESET}\t${CLR_VALUE}${info.pkg}${CLR_RESET}`
  ];

  const maxLogoWidth = Math.max(...logo.map(l => l.length));
  const totalLines = Math.max(logo.length, infoLines.length);

  for (let i = 0; i < totalLines; i++) {
    const logoLine = logo[i] || '';
    const infoLine = infoLines[i] || '';
    printR(padRight(logoLine, maxLogoWidth + 2) + infoLine + "\n");
  }

  const colors = [
  "\x1b[48;5;196m", // red
  "\x1b[48;5;202m", // red-orange
  "\x1b[48;5;208m", // orange
  "\x1b[48;5;214m", // amber
  "\x1b[48;5;220m", // yellow-orange
  "\x1b[48;5;226m", // yellow
  "\x1b[48;5;118m", // yellow-green
  "\x1b[48;5;82m",  // green
  "\x1b[48;5;49m",  // green-cyan
  "\x1b[48;5;45m",  // cyan
  "\x1b[48;5;33m",  // blue-cyan
  "\x1b[48;5;99m",  // violet
  "\x1b[48;5;27m",  // blue
  "\x1b[48;5;93m",  // magenta
  "\x1b[48;5;201m"  // pink
];

  let bar = "";
  for (const c of colors) bar += `${c}  ${CLR_RESET}`;
  printR("\n" + repeat(" ", maxLogoWidth + 2) + bar + "\n");
  function gradient(startRGB, endRGB, steps) {
    const [r1, g1, b1] = startRGB;
    const [r2, g2, b2] = endRGB;
    let out = "";
    for (let i = 0; i < steps; i++) {
      const t = i / (steps - 1);
      const r = Math.round(r1 + (r2 - r1) * t);
      const g = Math.round(g1 + (g2 - g1) * t);
      const b = Math.round(b1 + (b2 - b1) * t);
      out += `\x1b[48;2;${r};${g};${b}m `;
    }
    return out + CLR_RESET;
  }
  const smoothBar = gradient([0, 0, 0], [255, 255, 255], 30);
  printR(repeat(" ", maxLogoWidth + 2) + smoothBar + "\n");
}
