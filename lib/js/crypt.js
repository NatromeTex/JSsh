const crypt = {
  sha1(msg) {
    function rotateLeft(n, s) {
      return (n << s) | (n >>> (32 - s));
    }

    function toHexStr(n) {
      let str = "";
      for (let j = 7; j >= 0; j--) {
        str += ((n >>> (j * 4)) & 0x0f).toString(16);
      }
      return str;
    }

    let utf8Msg = unescape(encodeURIComponent(msg));

    const words = [];
    for (let i = 0; i < utf8Msg.length; i++) {
      words[i >> 2] |= utf8Msg.charCodeAt(i) << (24 - (i % 4) * 8);
    }

    const bitLength = utf8Msg.length * 8;
    words[bitLength >> 5] |= 0x80 << (24 - bitLength % 32);
    words[(((bitLength + 64) >>> 9) << 4) + 15] = bitLength;

    let h0 = 0x67452301;
    let h1 = 0xEFCDAB89;
    let h2 = 0x98BADCFE;
    let h3 = 0x10325476;
    let h4 = 0xC3D2E1F0;

    for (let i = 0; i < words.length; i += 16) {
      const w = [];
      let a = h0;
      let b = h1;
      let c = h2;
      let d = h3;
      let e = h4;

      for (let j = 0; j < 80; j++) {
        if (j < 16) {
          w[j] = words[i + j] || 0;
        } else {
          w[j] = rotateLeft(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        }

        let f, k;
        if (j < 20) {
          f = (b & c) | (~b & d);
          k = 0x5A827999;
        } else if (j < 40) {
          f = b ^ c ^ d;
          k = 0x6ED9EBA1;
        } else if (j < 60) {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8F1BBCDC;
        } else {
          f = b ^ c ^ d;
          k = 0xCA62C1D6;
        }

        const temp = (rotateLeft(a, 5) + f + e + k + w[j]) | 0;
        e = d;
        d = c;
        c = rotateLeft(b, 30);
        b = a;
        a = temp;
      }

      h0 = (h0 + a) | 0;
      h1 = (h1 + b) | 0;
      h2 = (h2 + c) | 0;
      h3 = (h3 + d) | 0;
      h4 = (h4 + e) | 0;
    }

    return (toHexStr(h0) + toHexStr(h1) + toHexStr(h2) + toHexStr(h3) + toHexStr(h4)).toUpperCase();
  },
  sha256(msg) {
    function rotateRight(n, s) {
      return (n >>> s) | (n << (32 - s));
    }

    function toHexStr(n) {
      let str = "";
      for (let j = 7; j >= 0; j--) {
        str += ((n >>> (j * 4)) & 0x0f).toString(16);
      }
      return str;
    }

    const K = [
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    ];

    let utf8Msg = unescape(encodeURIComponent(msg));

    const words = [];
    for (let i = 0; i < utf8Msg.length; i++) {
      words[i >> 2] |= utf8Msg.charCodeAt(i) << (24 - (i % 4) * 8);
    }

    const bitLength = utf8Msg.length * 8;
    words[bitLength >> 5] |= 0x80 << (24 - bitLength % 32);
    words[(((bitLength + 64) >>> 9) << 4) + 15] = bitLength;

    let H = [
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    ];

    for (let i = 0; i < words.length; i += 16) {
      const w = [];
      let a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];

      for (let t = 0; t < 64; t++) {
        if (t < 16) {
          w[t] = words[i + t] || 0;
        } else {
          const s0 = rotateRight(w[t - 15], 7) ^ rotateRight(w[t - 15], 18) ^ (w[t - 15] >>> 3);
          const s1 = rotateRight(w[t - 2], 17) ^ rotateRight(w[t - 2], 19) ^ (w[t - 2] >>> 10);
          w[t] = (w[t - 16] + s0 + w[t - 7] + s1) | 0;
        }

        const S1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        const ch = (e & f) ^ (~e & g);
        const temp1 = (h + S1 + ch + K[t] + w[t]) | 0;
        const S0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        const maj = (a & b) ^ (a & c) ^ (b & c);
        const temp2 = (S0 + maj) | 0;

        h = g;
        g = f;
        f = e;
        e = (d + temp1) | 0;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) | 0;
      }

      H[0] = (H[0] + a) | 0;
      H[1] = (H[1] + b) | 0;
      H[2] = (H[2] + c) | 0;
      H[3] = (H[3] + d) | 0;
      H[4] = (H[4] + e) | 0;
      H[5] = (H[5] + f) | 0;
      H[6] = (H[6] + g) | 0;
      H[7] = (H[7] + h) | 0;
    }

    let result = "";
    for (let i = 0; i < H.length; i++) {
      result += toHexStr(H[i]);
    }
    return result.toUpperCase();
  },
  sha512(msg) {
    // TODO: return sha512 hash of msg
  },
  randomBytes: function (length) {
    // TODO: generate secure random bytes
  }
};
