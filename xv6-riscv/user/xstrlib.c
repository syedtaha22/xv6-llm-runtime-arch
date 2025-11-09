// user/strutils.c
// Minimal user-space sprintf/sscanf/isprint/isspace for xv6
// - small, bounded vsnprintf/snprintf used by sprintf wrapper
// - limited sscanf supporting %x/%X (hex), %d, %f, %s, %c
// - isprint, isspace helpers
//
// included this in ULIB so user programs can link it
//
// uses xv6 types (uint). Uses <stdarg.h> for varargs.

#include "../kernel/types.h"
#include "user.h"
#include <stdarg.h>

/* ---------- isprint, isspace ---------- */

int xisprint(int c) {
  return (c >= 0x20 && c < 0x7f);
}

int xisspace(int c) {
  return (c == ' ' || c == '\f' || c == '\n' ||
          c == '\r' || c == '\t' || c == '\v');
}

/* ---------- small integer->string helper ---------- */

static int utoa_dec(unsigned long long val, char *buf, int bufsize) {
  if (bufsize <= 0) return 0;
  char tmp[32];
  int tp = 0;
  if (val == 0) {
    if (bufsize > 1) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    buf[0] = '\0'; return 0;
  }
  while (val && tp < (int)sizeof(tmp)) {
    tmp[tp++] = '0' + (val % 10);
    val /= 10;
  }
  int out = 0;
  for (int i = tp - 1; i >= 0 && out + 1 < bufsize; --i)
    buf[out++] = tmp[i];
  buf[out] = '\0';
  return out;
}

static int utoa_hex(unsigned long long val, char *buf, int bufsize, int uppercase) {
  if (bufsize <= 0) return 0;
  char tmp[32];
  int tp = 0;
  if (val == 0) {
    if (bufsize > 1) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    buf[0] = '\0'; return 0;
  }
  while (val && tp < (int)sizeof(tmp)) {
    int d = val & 0xF;
    if (d < 10) tmp[tp++] = '0' + d;
    else tmp[tp++] = (uppercase ? 'A' : 'a') + (d - 10);
    val >>= 4;
  }
  int out = 0;
  for (int i = tp - 1; i >= 0 && out + 1 < bufsize; --i)
    buf[out++] = tmp[i];
  buf[out] = '\0';
  return out;
}

/* ---------- float formatting (simple) ----------
   We implement a conservative ftoa that supports fixed-point decimal with precision.
   Uses simple rounding. Not locale-aware, but sufficient for "%.Nf".
*/

/*static double round_to_precision(double x, int prec) {
  double p = 1.0;
  for (int i = 0; i < prec; ++i) p *= 10.0;
  if (x >= 0) return (double)((long long)(x * p + 0.5)) / p;
  else return (double)((long long)(x * p - 0.5)) / p;
}*/

/*static int ftoa(double val, char *buf, int bufsize, int prec) {
  if (bufsize <= 0) return 0;
  if (prec < 0) prec = 6;
  if (val != val) { // NaN
    if (bufsize > 3) { memcpy(buf, "nan", 3); buf[3] = '\0'; return 3; }
    return 0;
  } 
  if (val == 1.0 / 0.0) { if (bufsize > 3) { memcpy(buf, "inf", 3); buf[3] = '\0'; return 3; } return 0; }
  if (val == -1.0 / 0.0) { if (bufsize > 4) { memcpy(buf, "-inf", 4); buf[4] = '\0'; return 4; } return 0; }

  int pos = 0;
  if (val < 0.0) {
    if (pos + 1 < bufsize) buf[pos++] = '-';
    val = -val;
  }

  val = round_to_precision(val, prec);
  unsigned long long ipart = (unsigned long long)val;
  double frac = val - (double)ipart;

  char intbuf[32];
  int nint = utoa_dec(ipart, intbuf, sizeof(intbuf));
  if (pos + nint + 1 >= bufsize) { // not enough for integer and possible dot
    int copy = bufsize - pos - 1;
    if (copy > 0) {
      memcpy(buf + pos, intbuf, copy);
      pos += copy;
    }
    buf[pos] = '\0';
    return pos;
  }
  memcpy(buf + pos, intbuf, nint);
  pos += nint;

  if (prec == 0) { buf[pos] = '\0'; return pos; }
  if (pos + 1 >= bufsize) { buf[pos] = '\0'; return pos; }

  buf[pos++] = '.';
  for (int i = 0; i < prec; ++i) {
    frac *= 10.0;
    int d = (int)frac;
    if (pos + 1 >= bufsize) break;
    buf[pos++] = '0' + d;
    frac -= d;
  }
  buf[pos] = '\0';
  return pos;
}*/

/* ---------- vsnprintf (bounded) ----------
   Supports: %d, %s, %f (with precision like %.4f), %c, %%, %x/%X
   Writes at most size-1 bytes + NUL. Returns number of bytes written (excluding NUL).
*/

int xvsnprintf(char *out, int size, const char *fmt, va_list ap) {
  if (size <= 0) return 0;
  int outpos = 0;
  const char *p = fmt;
  while (*p) {
    if (*p != '%') {
      if (outpos + 1 < size) out[outpos++] = *p;
      ++p;
      continue;
    }
    p++; // skip '%'
    // parse optional precision like .N
    int precision = -1;
    (void)precision;  //not used before fpu, so gcc flag
    if (*p == '.') {
      p++;
      int v = 0;
      while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
      precision = v;
    }
    // optional width is ignored except numeric padding for %02X in sscanf case (we don't implement width here)
    // parse spec
    char spec = *p++;
    if (spec == '%') {
      if (outpos + 1 < size) out[outpos++] = '%';
    } else if (spec == 'd') {
      int v = va_arg(ap, int);
      unsigned int uv = (v < 0) ? (unsigned int)(- (long long)v) : (unsigned int)v;
      char tmp[32];
      int n = utoa_dec(uv, tmp, sizeof(tmp));
      if (v < 0) {
        if (outpos + 1 < size) out[outpos++] = '-';
      }
      for (int i = 0; i < n && outpos + 1 < size; ++i) out[outpos++] = tmp[i];
    } else if (spec == 's') {
      char *s = va_arg(ap, char*);
      if (!s) s = "(null)";
      for (int i = 0; s[i] && outpos + 1 < size; ++i) out[outpos++] = s[i];
    } else if (spec == 'c') {
      int ch = va_arg(ap, int);
      if (outpos + 1 < size) out[outpos++] = (char)ch;
    } else if (spec == 'f') {
    //after float implementatin
      //double fv = va_arg(ap, double);
      //char tmp[64];
      //int used = ftoa(fv, tmp, sizeof(tmp), precision >= 0 ? precision : 6);
      //for (int i = 0; i < used && outpos + 1 < size; ++i) out[outpos++] = tmp[i];
      
    //before float implementation
      char *placeholder = "[float]";
      for (int i = 0; placeholder[i] && outpos + 1 < size; ++i)
        out[outpos++] = placeholder[i];
    } else if (spec == 'x' || spec == 'X') {
      unsigned int uv = va_arg(ap, unsigned int);
      char tmp[32];
      int used = utoa_hex(uv, tmp, sizeof(tmp), spec == 'X');
      for (int i = 0; i < used && outpos + 1 < size; ++i) out[outpos++] = tmp[i];
    } else {
      // unknown spec: print as-is (percent + char)
      if (outpos + 1 < size) out[outpos++] = '%';
      if (outpos + 1 < size) out[outpos++] = spec;
    }
  }
  out[outpos] = '\0';
  return outpos;
}

int xsnprintf(char *out, int size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = xvsnprintf(out, size, fmt, ap);
  va_end(ap);
  return r;
}

// sprintf wrapper: **bounded** internally to avoid unbounded overflow.
// We implement as calling vsnprintf with a large cap (1024) but keep real behavior
// for callers with large buffers. This prevents runaway writes in buggy callers.
int xsprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = xvsnprintf(out, 1024, fmt, ap);
  // Ensure NUL at safe place; if user buffer < r, it is user's fault; we keep NUL at pos min(r,1023)
  if (r >= 1024) out[1023] = '\0';
  va_end(ap);
  return r;
}

/* ---------- limited sscanf ----------
   Supports parsing formats with %d, %f, %s, %c, %x/%X (hex).
   For formats with literal characters (like "<0x%02hhX>") this implementation expects the
   format string to contain those literal chars and will match them.
   The 'hh' length modifier is ignored; integer results are written to int* or unsigned int* as usual.
   Returns number of successful assignments.
*/

int xsscanf(const char *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int assigned = 0;
  const char *ps = s;
  const char *pf = fmt;

  while (*pf) {
    if (*pf == '%') {
      pf++;
      // skip length modifiers we don't support (e.g., hh)
      if (*pf == 'h') {
        // support up to 'hh'
        pf++;
        if (*pf == 'h') pf++;
      }
      // optionally a width like 02 -- skip digits in format; we'll ignore widths when parsing
      while (*pf >= '0' && *pf <= '9') pf++;
      char spec = *pf++;
      // skip whitespace in input before conversions
      while (*ps && xisspace((int)*ps)) ps++;
      if (spec == 'd') {
        int sign = 1;
        long val = 0;
        if (*ps == '-') { sign = -1; ps++; }
        if (*ps < '0' || *ps > '9') { break; }
        while (*ps >= '0' && *ps <= '9') { val = val * 10 + (*ps - '0'); ps++; }
        int *ip = va_arg(ap, int*);
        *ip = (int)(val * sign);
        assigned++;
      } /*else if (spec == 'f') {
        // simple float parser: [sign]digits[.digits]
        int sign = 1;
        double val = 0.0;
        if (*ps == '-') { sign = -1; ps++; }
        if (!(*ps >= '0' && *ps <= '9')) break;
        while (*ps >= '0' && *ps <= '9') { val = val * 10.0 + (*ps - '0'); ps++; }
        if (*ps == '.') {
          ps++;
          double place = 0.1;
          while (*ps >= '0' && *ps <= '9') {
            val += (*ps - '0') * place;
            place *= 0.1;
            ps++;
          }
        }
        double *fp = va_arg(ap, double*);
        *fp = val * sign;
        assigned++; 
      }*/ else if (spec == 's') {
        char *dest = va_arg(ap, char*);
        if (!dest) break;
        while (*ps && !xisspace((int)*ps)) { *dest++ = *ps++; }
        *dest = '\0';
        assigned++;
      } else if (spec == 'c') {
        char *cp = va_arg(ap, char*);
        *cp = *ps ? *ps++ : '\0';
        assigned++;
      } else if (spec == 'x' || spec == 'X') {
        unsigned int val = 0;
        int got = 0;
        while ((*ps >= '0' && *ps <= '9') || (*ps >= 'a' && *ps <= 'f') || (*ps >= 'A' && *ps <= 'F')) {
          got = 1;
          int digit;
          if (*ps >= '0' && *ps <= '9') digit = *ps - '0';
          else if (*ps >= 'a' && *ps <= 'f') digit = 10 + (*ps - 'a');
          else digit = 10 + (*ps - 'A');
          val = (val << 4) | (unsigned int)digit;
          ps++;
        }
        if (!got) break;
        unsigned int *up = va_arg(ap, unsigned int*);
        *up = val;
        assigned++;
      } else {
        // unsupported specifier: abort
        break;
      }
    } else {
      // literal match: require fmt char to equal input char
      if (*pf == *ps) { pf++; ps++; }
      else break;
    }
    // skip any whitespace in fmt
    while (*pf == ' ') pf++;
  }

  va_end(ap);
  return assigned;
}
