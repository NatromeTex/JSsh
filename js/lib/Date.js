// Date
function date() {
  const d = new Date();
  const days = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
  const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

  const day = days[d.getDay()];
  const date = d.getDate();
  const suffix = (date % 10 === 1 && date !== 11) ? "st" :
    (date % 10 === 2 && date !== 12) ? "nd" :
      (date % 10 === 3 && date !== 13) ? "rd" : "th";

  const month = months[d.getMonth()];
  const year = d.getFullYear();

  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  const ss = String(d.getSeconds()).padStart(2, "0");

  // getTimezoneOffset() is minutes *behind* UTC, so invert sign
  const tzOffset = -d.getTimezoneOffset();
  const sign = tzOffset >= 0 ? "+" : "-";
  const hours = String(Math.floor(Math.abs(tzOffset) / 60)).padStart(2, "0");
  const mins = String(Math.abs(tzOffset) % 60).padStart(2, "0");

  return `It is ${day}, ${date}${suffix} ${month}, ${year} ${hh}:${mm}:${ss} GMT${sign}${hours}${mins}`;
}


// Export it globally into JSsh
globalThis.date = date;
