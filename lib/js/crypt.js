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

    const bytes = Array.from(this.normalizeInput(msg));

    const words = [];
    for (let i = 0; i < bytes.length; i++) {
      words[i >> 2] |= bytes[i] << (24 - (i % 4) * 8);
    }

    const bitLength = bytes.length * 8;
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

    const bytes = Array.from(this.normalizeInput(msg));

    const words = [];
    for (let i = 0; i < bytes.length; i++) {
      words[i >> 2] |= bytes[i] << (24 - (i % 4) * 8);
    }

    const bitLength = bytes.length * 8;
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
  sha384(msg) {
    // constants (K) as 64-bit BigInts (same as SHA-512)
    const K = [
      0x428a2f98d728ae22n, 0x7137449123ef65cdn, 0xb5c0fbcfec4d3b2fn, 0xe9b5dba58189dbbcn,
      0x3956c25bf348b538n, 0x59f111f1b605d019n, 0x923f82a4af194f9bn, 0xab1c5ed5da6d8118n,
      0xd807aa98a3030242n, 0x12835b0145706fben, 0x243185be4ee4b28cn, 0x550c7dc3d5ffb4e2n,
      0x72be5d74f27b896fn, 0x80deb1fe3b1696b1n, 0x9bdc06a725c71235n, 0xc19bf174cf692694n,
      0xe49b69c19ef14ad2n, 0xefbe4786384f25e3n, 0x0fc19dc68b8cd5b5n, 0x240ca1cc77ac9c65n,
      0x2de92c6f592b0275n, 0x4a7484aa6ea6e483n, 0x5cb0a9dcbd41fbd4n, 0x76f988da831153b5n,
      0x983e5152ee66dfabn, 0xa831c66d2db43210n, 0xb00327c898fb213fn, 0xbf597fc7beef0ee4n,
      0xc6e00bf33da88fc2n, 0xd5a79147930aa725n, 0x06ca6351e003826fn, 0x142929670a0e6e70n,
      0x27b70a8546d22ffcn, 0x2e1b21385c26c926n, 0x4d2c6dfc5ac42aedn, 0x53380d139d95b3dfn,
      0x650a73548baf63den, 0x766a0abb3c77b2a8n, 0x81c2c92e47edaee6n, 0x92722c851482353bn,
      0xa2bfe8a14cf10364n, 0xa81a664bbc423001n, 0xc24b8b70d0f89791n, 0xc76c51a30654be30n,
      0xd192e819d6ef5218n, 0xd69906245565a910n, 0xf40e35855771202an, 0x106aa07032bbd1b8n,
      0x19a4c116b8d2d0c8n, 0x1e376c085141ab53n, 0x2748774cdf8eeb99n, 0x34b0bcb5e19b48a8n,
      0x391c0cb3c5c95a63n, 0x4ed8aa4ae3418acbn, 0x5b9cca4f7763e373n, 0x682e6ff3d6b2b8a3n,
      0x748f82ee5defb2fcn, 0x78a5636f43172f60n, 0x84c87814a1f0ab72n, 0x8cc702081a6439ecn,
      0x90befffa23631e28n, 0xa4506cebde82bde9n, 0xbef9a3f7b2c67915n, 0xc67178f2e372532bn,
      0xca273eceea26619cn, 0xd186b8c721c0c207n, 0xeada7dd6cde0eb1en, 0xf57d4f7fee6ed178n,
      0x06f067aa72176fban, 0x0a637dc5a2c898a6n, 0x113f9804bef90daen, 0x1b710b35131c471bn,
      0x28db77f523047d84n, 0x32caab7b40c72493n, 0x3c9ebe0a15c9bebcn, 0x431d67c49c100d4cn,
      0x4cc5d4becb3e42b6n, 0x597f299cfc657e2an, 0x5fcb6fab3ad6faecn, 0x6c44198c4a475817n
    ];

    // SHA-384 initial hash values (from FIPS 180-4)
    const H_INIT = [
      0xcbbb9d5dc1059ed8n, 0x629a292a367cd507n, 0x9159015a3070dd17n, 0x152fecd8f70e5939n,
      0x67332667ffc00b31n, 0x8eb44a8768581511n, 0xdb0c2e0d64f98fa7n, 0x47b5481dbefa4fa4n
    ];

    const mask = (1n << 64n) - 1n;
    const rotr = (x, r) => ((x >> BigInt(r)) | (x << (64n - BigInt(r)))) & mask;

    // UTF-8 encode (uses your normalizeInput)
    const bytes = Array.from(this.normalizeInput(msg));

    // padding (same as SHA-512)
    const bitLen = BigInt(bytes.length) * 8n;
    bytes.push(0x80);
    while ((bytes.length % 128) !== 112) bytes.push(0);
    const highLen = bitLen >> 64n;
    const lowLen = bitLen & mask;
    for (let i = 7; i >= 0; i--) bytes.push(Number((highLen >> BigInt(i * 8)) & 0xffn));
    for (let i = 7; i >= 0; i--) bytes.push(Number((lowLen >> BigInt(i * 8)) & 0xffn));

    // processing
    const H = H_INIT.slice();
    for (let chunk = 0; chunk < bytes.length; chunk += 128) {
      const w = new Array(80);
      for (let t = 0; t < 16; t++) {
        let x = 0n;
        for (let b = 0; b < 8; b++) x = (x << 8n) | BigInt(bytes[chunk + t * 8 + b]);
        w[t] = x;
      }
      for (let t = 16; t < 80; t++) {
        const s0 = rotr(w[t - 15], 1) ^ rotr(w[t - 15], 8) ^ (w[t - 15] >> 7n);
        const s1 = rotr(w[t - 2], 19) ^ rotr(w[t - 2], 61) ^ (w[t - 2] >> 6n);
        w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & mask;
      }

      let a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (let t = 0; t < 80; t++) {
        const S1 = rotr(e, 14) ^ rotr(e, 18) ^ rotr(e, 41);
        const ch = (e & f) ^ ((~e) & g);
        const temp1 = (h + S1 + ch + K[t] + w[t]) & mask;
        const S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39);
        const maj = (a & b) ^ (a & c) ^ (b & c);
        const temp2 = (S0 + maj) & mask;

        h = g;
        g = f;
        f = e;
        e = (d + temp1) & mask;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) & mask;
      }

      H[0] = (H[0] + a) & mask;
      H[1] = (H[1] + b) & mask;
      H[2] = (H[2] + c) & mask;
      H[3] = (H[3] + d) & mask;
      H[4] = (H[4] + e) & mask;
      H[5] = (H[5] + f) & mask;
      H[6] = (H[6] + g) & mask;
      H[7] = (H[7] + h) & mask;
    }

    // produce 384-bit hex (first 6 words)
    let out = '';
    for (let i = 0; i < 6; i++) out += H[i].toString(16).padStart(16, '0');
    return out.toUpperCase();
  },
  sha512(msg) {
    // constants (K) and initial hash (H) as 64-bit BigInts
    const K = [
      0x428a2f98d728ae22n, 0x7137449123ef65cdn, 0xb5c0fbcfec4d3b2fn, 0xe9b5dba58189dbbcn,
      0x3956c25bf348b538n, 0x59f111f1b605d019n, 0x923f82a4af194f9bn, 0xab1c5ed5da6d8118n,
      0xd807aa98a3030242n, 0x12835b0145706fben, 0x243185be4ee4b28cn, 0x550c7dc3d5ffb4e2n,
      0x72be5d74f27b896fn, 0x80deb1fe3b1696b1n, 0x9bdc06a725c71235n, 0xc19bf174cf692694n,
      0xe49b69c19ef14ad2n, 0xefbe4786384f25e3n, 0x0fc19dc68b8cd5b5n, 0x240ca1cc77ac9c65n,
      0x2de92c6f592b0275n, 0x4a7484aa6ea6e483n, 0x5cb0a9dcbd41fbd4n, 0x76f988da831153b5n,
      0x983e5152ee66dfabn, 0xa831c66d2db43210n, 0xb00327c898fb213fn, 0xbf597fc7beef0ee4n,
      0xc6e00bf33da88fc2n, 0xd5a79147930aa725n, 0x06ca6351e003826fn, 0x142929670a0e6e70n,
      0x27b70a8546d22ffcn, 0x2e1b21385c26c926n, 0x4d2c6dfc5ac42aedn, 0x53380d139d95b3dfn,
      0x650a73548baf63den, 0x766a0abb3c77b2a8n, 0x81c2c92e47edaee6n, 0x92722c851482353bn,
      0xa2bfe8a14cf10364n, 0xa81a664bbc423001n, 0xc24b8b70d0f89791n, 0xc76c51a30654be30n,
      0xd192e819d6ef5218n, 0xd69906245565a910n, 0xf40e35855771202an, 0x106aa07032bbd1b8n,
      0x19a4c116b8d2d0c8n, 0x1e376c085141ab53n, 0x2748774cdf8eeb99n, 0x34b0bcb5e19b48a8n,
      0x391c0cb3c5c95a63n, 0x4ed8aa4ae3418acbn, 0x5b9cca4f7763e373n, 0x682e6ff3d6b2b8a3n,
      0x748f82ee5defb2fcn, 0x78a5636f43172f60n, 0x84c87814a1f0ab72n, 0x8cc702081a6439ecn,
      0x90befffa23631e28n, 0xa4506cebde82bde9n, 0xbef9a3f7b2c67915n, 0xc67178f2e372532bn,
      0xca273eceea26619cn, 0xd186b8c721c0c207n, 0xeada7dd6cde0eb1en, 0xf57d4f7fee6ed178n,
      0x06f067aa72176fban, 0x0a637dc5a2c898a6n, 0x113f9804bef90daen, 0x1b710b35131c471bn,
      0x28db77f523047d84n, 0x32caab7b40c72493n, 0x3c9ebe0a15c9bebcn, 0x431d67c49c100d4cn,
      0x4cc5d4becb3e42b6n, 0x597f299cfc657e2an, 0x5fcb6fab3ad6faecn, 0x6c44198c4a475817n
    ];
    const H_INIT = [
      0x6a09e667f3bcc908n, 0xbb67ae8584caa73bn, 0x3c6ef372fe94f82bn, 0xa54ff53a5f1d36f1n,
      0x510e527fade682d1n, 0x9b05688c2b3e6c1fn, 0x1f83d9abfb41bd6bn, 0x5be0cd19137e2179n
    ];
    const mask = (1n << 64n) - 1n;
    const rotr = (x, r) => ((x >> BigInt(r)) | (x << (64n - BigInt(r)))) & mask;

    // UTF-8 encode (manual, handles surrogates)
    const bytes = Array.from(this.normalizeInput(msg));

    // padding
    const bitLen = BigInt(bytes.length) * 8n;
    bytes.push(0x80);
    while ((bytes.length % 128) !== 112) bytes.push(0);
    const highLen = bitLen >> 64n;
    const lowLen = bitLen & mask;
    for (let i = 7; i >= 0; i--) bytes.push(Number((highLen >> BigInt(i * 8)) & 0xffn));
    for (let i = 7; i >= 0; i--) bytes.push(Number((lowLen >> BigInt(i * 8)) & 0xffn));

    // processing
    const H = H_INIT.slice();
    for (let chunk = 0; chunk < bytes.length; chunk += 128) {
      const w = new Array(80);
      for (let t = 0; t < 16; t++) {
        let x = 0n;
        for (let b = 0; b < 8; b++) x = (x << 8n) | BigInt(bytes[chunk + t * 8 + b]);
        w[t] = x;
      }
      for (let t = 16; t < 80; t++) {
        const s0 = rotr(w[t - 15], 1) ^ rotr(w[t - 15], 8) ^ (w[t - 15] >> 7n);
        const s1 = rotr(w[t - 2], 19) ^ rotr(w[t - 2], 61) ^ (w[t - 2] >> 6n);
        w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & mask;
      }

      let a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (let t = 0; t < 80; t++) {
        const S1 = rotr(e, 14) ^ rotr(e, 18) ^ rotr(e, 41);
        const ch = (e & f) ^ ((~e) & g);
        const temp1 = (h + S1 + ch + K[t] + w[t]) & mask;
        const S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39);
        const maj = (a & b) ^ (a & c) ^ (b & c);
        const temp2 = (S0 + maj) & mask;

        h = g;
        g = f;
        f = e;
        e = (d + temp1) & mask;
        d = c;
        c = b;
        b = a;
        a = (temp1 + temp2) & mask;
      }

      H[0] = (H[0] + a) & mask;
      H[1] = (H[1] + b) & mask;
      H[2] = (H[2] + c) & mask;
      H[3] = (H[3] + d) & mask;
      H[4] = (H[4] + e) & mask;
      H[5] = (H[5] + f) & mask;
      H[6] = (H[6] + g) & mask;
      H[7] = (H[7] + h) & mask;
    }

    // produce hex
    let out = '';
    for (let i = 0; i < 8; i++) out += H[i].toString(16).padStart(16, '0');
    return out.toUpperCase();
  },
  md5(msg) {
    function rotateLeft(n, s) {
      return (n << s) | (n >>> (32 - s));
    }
    function toLittleEndianHex(n) {
      let hex = "";
      for (let i = 0; i < 4; i++) {
        hex += ("0" + ((n >> (i * 8)) & 0xFF).toString(16)).slice(-2);
      }
      return hex;
    }
    const T = [
      0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
      0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
      0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
      0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    ];
    const S = [
      7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
      5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    ];

    // Convert input to byte array
    let bytes = this.normalizeInput(msg);

    // Convert bytes to 32-bit words
    const words = [];
    for (let i = 0; i < bytes.length; i++) {
      words[i >> 2] |= bytes[i] << ((i % 4) * 8);
    }

    const bitLength = bytes.length * 8;
    words[bitLength >> 5] |= 0x80 << (bitLength % 32);
    words[(((bitLength + 64) >>> 9) << 4) + 14] = bitLength;

    let a = 0x67452301;
    let b = 0xEFCDAB89;
    let c = 0x98BADCFE;
    let d = 0x10325476;

    for (let i = 0; i < words.length; i += 16) {
      let AA = a, BB = b, CC = c, DD = d;
      for (let j = 0; j < 64; j++) {
        let F, g;
        if (j < 16) {
          F = (b & c) | (~b & d);
          g = j;
        } else if (j < 32) {
          F = (d & b) | (~d & c);
          g = (1 + 5 * j) % 16;
        } else if (j < 48) {
          F = b ^ c ^ d;
          g = (5 + 3 * j) % 16;
        } else {
          F = c ^ (b | ~d);
          g = (7 * j) % 16;
        }
        const M = words[i + g] || 0;
        const temp = d;
        d = c;
        c = b;
        b = (b + rotateLeft((a + F + T[j] + M), S[j])) | 0;
        a = temp;
      }
      a = (a + AA) | 0;
      b = (b + BB) | 0;
      c = (c + CC) | 0;
      d = (d + DD) | 0;
    }

    let str = toLittleEndianHex(a) + toLittleEndianHex(b) + toLittleEndianHex(c) + toLittleEndianHex(d);
    return str.toUpperCase();
  },
  base64(msg, mode) {
    const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (mode === 0) {
      // encode
      let utf8 = [];
      for (let i = 0; i < msg.length; i++) {
        let code = msg.charCodeAt(i);
        if (code < 0x80) utf8.push(code);
        else if (code < 0x800) {
          utf8.push(0xc0 | (code >> 6), 0x80 | (code & 0x3f));
        } else if (code >= 0xd800 && code <= 0xdbff) {
          // surrogate pair
          let hi = code, lo = msg.charCodeAt(++i);
          let cp = ((hi - 0xd800) << 10) + (lo - 0xdc00) + 0x10000;
          utf8.push(
            0xf0 | (cp >> 18),
            0x80 | ((cp >> 12) & 0x3f),
            0x80 | ((cp >> 6) & 0x3f),
            0x80 | (cp & 0x3f)
          );
        } else {
          utf8.push(0xe0 | (code >> 12), 0x80 | ((code >> 6) & 0x3f), 0x80 | (code & 0x3f));
        }
      }
      let out = "";
      for (let i = 0; i < utf8.length; i += 3) {
        let a = utf8[i] ?? 0;
        let b = utf8[i + 1] ?? 0;
        let c = utf8[i + 2] ?? 0;
        let n = (a << 16) | (b << 8) | c;
        out += chars[(n >> 18) & 63] + chars[(n >> 12) & 63] +
          (i + 1 < utf8.length ? chars[(n >> 6) & 63] : "=") +
          (i + 2 < utf8.length ? chars[n & 63] : "=");
      }
      return out;
    } else {
      // decode
      let clean = msg.replace(/[^A-Za-z0-9+/=]/g, "");
      let bytes = [];
      for (let i = 0; i < clean.length; i += 4) {
        let n = (chars.indexOf(clean[i]) << 18) |
          (chars.indexOf(clean[i + 1]) << 12) |
          ((chars.indexOf(clean[i + 2]) & 63) << 6) |
          (chars.indexOf(clean[i + 3]) & 63);
        bytes.push((n >> 16) & 255, (n >> 8) & 255, n & 255);
      }
      if (clean.endsWith("==")) bytes = bytes.slice(0, -2);
      else if (clean.endsWith("=")) bytes = bytes.slice(0, -1);

      let out = "";
      for (let i = 0; i < bytes.length;) {
        let b1 = bytes[i++];
        if (b1 < 0x80) out += String.fromCharCode(b1);
        else if (b1 < 0xe0) {
          let b2 = bytes[i++];
          out += String.fromCharCode(((b1 & 31) << 6) | (b2 & 63));
        } else if (b1 < 0xf0) {
          let b2 = bytes[i++], b3 = bytes[i++];
          out += String.fromCharCode(((b1 & 15) << 12) | ((b2 & 63) << 6) | (b3 & 63));
        } else {
          let b2 = bytes[i++], b3 = bytes[i++], b4 = bytes[i++];
          let cp = ((b1 & 7) << 18) | ((b2 & 63) << 12) | ((b3 & 63) << 6) | (b4 & 63);
          cp -= 0x10000;
          out += String.fromCharCode(0xd800 + (cp >> 10), 0xdc00 + (cp & 1023));
        }
      }
      return out;
    }
  },
  hmac(algo, msg, key) {
    // pick the right hash
    const hashFn = this[algo];
    if (typeof hashFn !== "function")
      throw new Error("unknown hash algorithm: " + algo);

    // normalize inputs
    const msgBytes = new Uint8Array(this.normalizeInput(msg));
    const keyBytes = new Uint8Array(this.normalizeInput(key));

    // block size in bytes
    const blockSize =
      algo.startsWith("sha512") || algo.startsWith("sha384") ? 128 : 64;

    // shorten or pad key
    let k = keyBytes;
    if (k.length > blockSize) {
      const hashed = this[algo](k);
      k = new Uint8Array(this.hexToBytes(hashed));
    }
    if (k.length < blockSize) {
      const tmp = new Uint8Array(blockSize);
      tmp.set(k);
      k = tmp;
    }

    // ipad and opad
    const ipad = new Uint8Array(blockSize);
    const opad = new Uint8Array(blockSize);
    for (let i = 0; i < blockSize; i++) {
      ipad[i] = k[i] ^ 0x36;
      opad[i] = k[i] ^ 0x5c;
    }

    // inner hash: H(ipad || msg)
    const inner = this[algo](this.concatBytes(ipad, msgBytes));

    // outer hash: H(opad || inner)
    const innerBytes = new Uint8Array(this.hexToBytes(inner));
    const final = this[algo](this.concatBytes(opad, innerBytes));

    return final;
  },

  compare: function (algo, target, hash) {
    hash = hash.toUpperCase()
    let Thash = ""
    if (algo === "sha1") {
      Thash = this.sha1(target)
    } else if (algo === "sha256") {
      Thash = this.sha256(target)
    } else if (algo === "sha512") {
      Thash = this.sha512(target)
    } else if (algo === "sha384") {
      Thash = this.sha384(target)
    } else if (algo === "md5") {
      Thash = this.md5(target)
    } else {
      throw new Error("Cannot find algorithm specified")
    }
    return Thash == hash
  },
  byteDump(path) {
    const fd = sys.open(path, 0, 0);
    if (fd < 0) throw new Error("cannot open file: " + path);

    const chunks = [];
    const bufsize = 4096;

    try {
      while (true) {
        const chunk = sys.read(fd, bufsize);
        if (!chunk || chunk.byteLength === 0) break;
        chunks.push(new Uint8Array(chunk));
      }
    } finally {
      sys.close(fd);
    }

    const totalLength = chunks.reduce((sum, c) => sum + c.length, 0);
    const result = new Uint8Array(totalLength);
    let offset = 0;

    for (const c of chunks) {
      result.set(c, offset);
      offset += c.length;
    }

    return result;
  },
  randomBytes(n) {
    const fd = sys.open("/dev/urandom", 0, 0);
    if (fd < 0) throw new Error("cannot open /dev/urandom");

    const buf = new Uint8Array(n);
    let offset = 0;

    try {
      while (offset < n) {
        const chunk = sys.read(fd, n - offset);
        if (!chunk || chunk.byteLength === 0)
          throw new Error("short read from /dev/urandom");
        const u8 = new Uint8Array(chunk);
        buf.set(u8, offset);
        offset += u8.length;
      }
    } finally {
      sys.close(fd);
    }

    return buf;
  },

  // helpers
  normalizeInput(msg) {
    if (typeof msg === 'string') {
      const utf8Msg = unescape(encodeURIComponent(msg));
      const bytes = new Uint8Array(utf8Msg.length);
      for (let i = 0; i < utf8Msg.length; i++) {
        bytes[i] = utf8Msg.charCodeAt(i);
      }
      return bytes;
    }
    if (msg instanceof Uint8Array) {
      return msg;
    }
    throw new Error('Input must be a string or Uint8Array');
  },
  hexToBytes(hex) {
    const out = new Uint8Array(hex.length / 2);
    for (let i = 0; i < out.length; i++)
      out[i] = parseInt(hex.substr(i * 2, 2), 16);
    return out;
  },
  concatBytes(a, b) {
    const out = new Uint8Array(a.length + b.length);
    out.set(a);
    out.set(b, a.length);
    return out;
  }
};
