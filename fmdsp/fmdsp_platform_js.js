export function imports() {
  return {
    cpuUsage() {
      return 0; // Not possible generically
    },

    nanotime() {
      return BigInt(Math.round(performance.now() * 1_000_000));
    },
  };
}
